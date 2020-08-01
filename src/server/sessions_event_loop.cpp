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
    unsigned long mem_max = 4 * 1024 * 1024; // 4Mb
#ifndef ENABLE_STRESS_TEST
    FILE *f = fopen("/proc/sys/net/core/rmem_max", "r");
    if (f) {
        fscanf(f, "%lu", &mem_max);
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

    m_eventFd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    epoll_event event;
    event.data.ptr = nullptr;
    event.data.fd = m_eventFd;
    event.events = EPOLLIN | EPOLLPRI | EPOLLRDHUP | EPOLLET;
    if (epoll_ctl(m_epollHandler, EPOLL_CTL_ADD, m_eventFd, &event))
        throw std::runtime_error{"Can't register the event handler"};

    m_loopThread = std::thread([this]{loop();});

//    // set insane priority ?
//    sched_param sch;
//    sch.sched_priority = sched_get_priority_max(SCHED_RR);
//    pthread_setschedparam(m_loopThread.native_handle(), SCHED_RR, &sch);
    TRACE(serverLogger) << "SessionsEventLoop::SessionsEventLoop " << this << " shared buffer mem_max: " << mem_max << " activeSessions = " << activeSessions();
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
        close(m_eventFd);
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

void SessionsEventLoop::setWorkloadBalancing(bool on)
{
    m_workloadBalancing = on;
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
    using clock = std::chrono::high_resolution_clock;
    using TimePoint = std::chrono::time_point<clock>;
    auto events = std::make_unique<epoll_event[]>(EventsSize);
    Ms timeout(1s); // Initial timeout
    while (!m_quit) {
        auto before = clock::now();
        bool wokeup = false;
        int triggeredEvents = epoll_wait(m_epollHandler, events.get(), EventsSize, timeout.count());
        if (triggeredEvents < 0)
            continue;
        if (!m_workloadBalancing) {
            for (int i = 0 ; i < triggeredEvents; ++i) {
                auto &event = events[i];
                if (event.data.fd != m_eventFd)
                    reinterpret_cast<ServerSession *>(event.data.ptr)->processEvents(event.events);
                wokeup = true;
            }
        } else {
            std::vector<std::pair<ServerSession *, uint32_t>> sessionEvents;
            sessionEvents.reserve(triggeredEvents);
            for (int i = 0 ; i < triggeredEvents; ++i) {
                auto &event = events[i];
                if (event.data.fd != m_eventFd) {
                    auto ptr = reinterpret_cast<ServerSession *>(event.data.ptr);
                    auto evs = event.events;
                    std::pair<ServerSession *, uint32_t> event{ptr, evs};
                    sessionEvents.insert(std::upper_bound(sessionEvents.begin(), sessionEvents.end(), event,
                                                          [](auto a, auto b) {
                        return a.first->order() < b.first->order();
                    }),
                                         event);
                } else {
                    wokeup = true;
                }
            }
            for (auto event : sessionEvents)
                event.first->processEvents(event.second);
        }

        auto now = clock::now();
        auto duration = std::chrono::duration_cast<Ms>(now - before);
        if ( duration < timeout && !wokeup) {
            timeout -= duration;
        } else {
            std::unordered_set<ServerSession *> wokeupsessions;
            if (wokeup) {
                uint64_t data;
                while(eventfd_read(m_eventFd, &data) == 0)
                    wokeupsessions.insert(reinterpret_cast<ServerSession *>(data));
            }
            // Some session(s) have timeout
            timeout = 1s; // maximum timeout
            std::unique_lock<std::mutex> lock{m_sessionsMutex};
            std::vector<ServerSession *> sessions;
            sessions.reserve(m_sessions.size());
            for (auto session : m_sessions)
                sessions.push_back(session);
            lock.unlock(); // Allow the server to insert new connections

            auto now = clock::now();
            for (auto session : sessions) {
                if (wokeup && wokeupsessions.find(session) != wokeupsessions.end())
                    session->wakeUp();
                auto sessionTimeout = session->nextTimeout();
                if (sessionTimeout != TimePoint{} && sessionTimeout <= now)
                    session->timeout();
                else // round to 50ms
                    timeout = std::min(timeout, std::max(50ms, std::chrono::duration_cast<Ms>(sessionTimeout - now)));
            }
        }

        // Delete all deleteLater pending sessions
        std::unique_lock<SpinLock> lock{m_deleteLaterMutex};
        for (auto &obj : m_deleteLaterObjects)
            delete obj;
        m_deleteLaterObjects.clear();
    }
}

} // namespace Getodac
