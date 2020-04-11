/*
    Copyright (C) 2020, BogDan Vatra <bogdan@kde.org>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as
    published by the Free Software Foundation, either version 3 of the
    License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <unistd.h>

#include <sys/epoll.h>
#include <sys/time.h>

#include <chrono>
#include <cstdio>
#include <iostream>

#include "server_logger.h"
#include "server_session.h"
#include "sessions_event_loop.h"

using namespace std::chrono_literals;

namespace Getodac {

namespace {
    const uint32_t EventsSize = 10000;
}

/*!
 * \brief SessionsEventLoop::SessionsEventLoop
 *
 * Creates a new EPOLL event loop
 */
SessionsEventLoop::SessionsEventLoop()
{
    unsigned long mem_min, mem_default, mem_max = 4194304; // 4Mb
#ifndef ENABLE_STRESS_TEST
    FILE *f = fopen("/proc/sys/net/ipv4/tcp_rmem", "r");
    if (f) {
        fscanf(f, "%lu %lu %lu", &mem_min, &mem_default, &mem_max);
        fclose(f);
    }
#else
    mem_max = 8; // super small bufer needed to test the partial parsing
#endif

    // This buffer is shared by all ServerSessions which are server
    // by this event loop to read the incoming data without
    // allocating any memory
    sharedReadBuffer.resize(mem_max);

    m_epollHandler = epoll_create1(EPOLL_CLOEXEC);
    if (m_epollHandler == -1)
        throw std::runtime_error{"Can't create epool handler"};

    m_loopThread = std::thread([this]{loop();});

//    // set insane priority ?
//    sched_param sch;
//    sch.sched_priority = sched_get_priority_max(SCHED_RR);
//    pthread_setschedparam(m_loopThread.native_handle(), SCHED_RR, &sch);
    TRACE(serverLogger) << "SessionsEventLoop::SessionsEventLoop " << this << " tcp_rmax_mem: " << mem_max << " activeSessions = " << activeSessions();
}

SessionsEventLoop::~SessionsEventLoop()
{
    try {
        // Quit event loop
        m_quit.store(true);
        m_loopThread.join();

        // Destroy all registered sessions
        std::unique_lock<std::mutex> lock{m_sessionsMutex};
        auto it = m_sessions.begin();
        while (it != m_sessions.end()) {
            auto session = it++;
            lock.unlock();
            delete (*session);
            lock.lock();
        }
    } catch (...) {}
    TRACE(serverLogger) << "SessionsEventLoop::~SessionsEventLoop " << this;
}

/*!
 * \brief SessionsEventLoop::registerSession
 *
 * Register a new ServerSession to this even loop
 *
 * \param session to register
 * \param events the events that epoll will listen for
 */
void SessionsEventLoop::registerSession(ServerSession *session, uint32_t events)
{
    {
        std::unique_lock<std::mutex> lock{m_sessionsMutex};
        // check http://en.cppreference.com/w/cpp/container/unordered_set/insert
        m_sessionsRehashed= m_sessions.size() == m_sessions.max_load_factor() * m_sessions.bucket_count();
        m_sessions.insert(session);
    }
    epoll_event event;
    event.data.ptr = session;
    event.events = events;
    if (epoll_ctl(m_epollHandler, EPOLL_CTL_ADD, session->sock(), &event))
        throw std::runtime_error{"Can't register session"};
    ++m_activeSessions;
    TRACE(serverLogger) << "SessionsEventLoop::registerSession(" << session << ", " << events << "), activeSessions = " << activeSessions();
}

/*!
 * \brief SessionsEventLoop::updateSession
 *
 * Updates a registered ServerSession
 *
 * \param session to update
 * \param events new epoll events
 */
void SessionsEventLoop::updateSession(ServerSession *session, uint32_t events)
{
    epoll_event event;
    event.data.ptr = session;
    event.events = events;
    if (epoll_ctl(m_epollHandler, EPOLL_CTL_MOD, session->sock(), &event))
        throw std::runtime_error{"Can't change the session"};
    TRACE(serverLogger) << "SessionsEventLoop::registerSession(" << session << ", " << events << ")";
}

/*!
 * \brief SessionsEventLoop::unregisterSession
 *
 * \param session to unregister
 */
void SessionsEventLoop::unregisterSession(ServerSession *session)
{
    {
        std::unique_lock<std::mutex> lock{m_sessionsMutex};
        m_sessions.erase(session);
        m_sessionsRehashed = true;
    }
    if (epoll_ctl(m_epollHandler, EPOLL_CTL_DEL, session->sock(), nullptr))
        throw std::runtime_error{"Can't remove the session"};
    --m_activeSessions;
    TRACE(serverLogger) << "SessionsEventLoop::unregisterSession(" << session << "), activeSessions = " << activeSessions();
}

/*!
 * \brief SessionsEventLoop::deleteLater
 *
 * Postpones a session for deletion
 *
 * \param session to delete later
 */
void SessionsEventLoop::deleteLater(ServerSession *session) noexcept
{
    // Idea "stolen" from Qt :)
    std::unique_lock<SpinLock> lock{m_deleteLaterMutex};
    try {
        m_deleteLaterObjects.insert(session);
    } catch (...) {}
}

/*!
 * \brief SessionsEventLoop::sharedReadBuffer
 *
 * Quite useful buffer, used by all ServerSession to temporary read all sock data.
 */

/*!
 * \brief SessionsEventLoop::activeSessions
 *
 * \return returns the number of active sessions on this loop
 */


/*!
 * \brief SessionsEventLoop::shutdown
 *
 * Asynchronously quits the event loop.
 */


void SessionsEventLoop::loop()
{
    using Ms = std::chrono::milliseconds;
    auto events = std::make_unique<epoll_event[]>(EventsSize);
    Ms timeout(1s); // Initial timeout
    while (!m_quit) {
        auto before = std::chrono::high_resolution_clock::now();
        int triggeredEvents = epoll_wait(m_epollHandler, events.get(), EventsSize, timeout.count());
        for (int i = 0 ; i < triggeredEvents; ++i)
            reinterpret_cast<ServerSession *>(events[i].data.ptr)->processEvents(events[i].events);

        auto now = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<Ms>(now - before);
        if ( duration < timeout) {
            timeout -= duration;
        } else {
            // Some session(s) have timeout
            timeout = 1s; // maximum timeout
            std::unique_lock<std::mutex> lock{m_sessionsMutex};
            do {
                m_sessionsRehashed = false; // reset sessionsRehashed flag

                auto it = m_sessions.begin();
                while (!m_sessionsRehashed && it != m_sessions.end()) {
                    auto session = it++;
                    lock.unlock(); // Allow the server to insert new connections
                    auto sessionTimeout = (*session)->nextTimeout();
                    if (sessionTimeout <= now)
                        (*session)->timeout();
                    else       // round to 100ms
                        timeout = std::min(timeout, std::max(100ms, std::chrono::duration_cast<Ms>(sessionTimeout - now)));
                    lock.lock();
                }
            } while (m_sessionsRehashed);
        }

        // Delete all deleteLater pending sessions
        std::unique_lock<SpinLock> lock{m_deleteLaterMutex};
        for (auto &obj : m_deleteLaterObjects)
            delete obj;
        m_deleteLaterObjects.clear();
    }
}

} // namespace Getodac
