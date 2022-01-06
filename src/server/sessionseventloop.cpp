/*
    Copyright (C) 2022, BogDan Vatra <bogdan@kde.org>

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
#include <sys/eventfd.h>
#include <sys/time.h>

#include <chrono>
#include <cstdio>
#include <iostream>

#include "serverlogger.h"
#include "serversession.h"
#include "sessionseventloop.h"

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
static unsigned long readProc(const char *path)
{
    unsigned long value = 4 * 1024 * 1024; // 4Mb
    FILE *f = fopen(path, "r");
    if (f) {
        fscanf(f, "%lu", &value);
        fclose(f);
    }
    return value;
}

SessionsEventLoop::SessionsEventLoop()
{
    unsigned long rmem_max = readProc("/proc/sys/net/core/rmem_max");

    // This buffer is shared by all basic_server_sessions which are server
    // by this event loop to read the incoming data without
    // allocating any memory
    sharedReadBuffer.resize(rmem_max);
    m_sharedWriteBuffer = std::make_shared<Dracon::CharBuffer>(readProc("/proc/sys/net/core/wmem_max"));

    m_epollHandler = epoll_create1(EPOLL_CLOEXEC);
    if (m_epollHandler == -1)
        throw std::runtime_error{"Can't create epool handler"};

    m_eventFd = eventfd(0, EFD_NONBLOCK);
    epoll_event event;
    event.data.ptr = nullptr;
    event.data.fd = m_eventFd;
    event.events = EPOLLHUP | EPOLLERR | EPOLLIN | EPOLLET;
    if (epoll_ctl(m_epollHandler, EPOLL_CTL_ADD, m_eventFd, &event))
        throw std::runtime_error{"Can't register the event handler"};

    m_loopThread = std::thread([this]{loop();});

//    // set insane priority ?
//    sched_param sch;
//    sch.sched_priority = sched_get_priority_max(SCHED_RR);
//    pthread_setschedparam(m_loopThread.native_handle(), SCHED_RR, &sch);
    TRACE(ServerLogger) << this << " shared buffer mem_max: " << rmem_max << " eventfd = " << m_eventFd;
}

SessionsEventLoop::~SessionsEventLoop()
{
    shutdown();
    try {
        // Quit event loop
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
    TRACE(ServerLogger) << this;
}

/*!
 * \brief SessionsEventLoop::registerSession
 *
 * Register a new basic_server_session to this even loop
 *
 * \param session to register
 * \param events the events that epoll will listen for
 */
void SessionsEventLoop::registerSession(BasicServerSession *session, uint32_t events)
{
    TRACE(ServerLogger) << session << " events" << events << activeSessions();
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
}

/*!
 * \brief SessionsEventLoop::updateSession
 *
 * Updates a registered basic_server_session
 *
 * \param session to update
 * \param events new epoll events
 */
void SessionsEventLoop::updateSession(BasicServerSession *session, uint32_t events)
{
    TRACE(ServerLogger) << session << " events:" << events;
    epoll_event event;
    event.data.ptr = session;
    event.events = events;
    if (epoll_ctl(m_epollHandler, EPOLL_CTL_MOD, session->sock(), &event))
        throw std::runtime_error{"Can't change the session"};
}

/*!
 * \brief SessionsEventLoop::unregisterSession
 *
 * \param session to unregister
 */
void SessionsEventLoop::unregisterSession(BasicServerSession *session)
{
    TRACE(ServerLogger) << session << " activeSessions:" << activeSessions();
    {
        std::unique_lock<std::mutex> lock{m_sessionsMutex};
        if (!m_sessions.erase(session))
            return;
    }
    if (epoll_ctl(m_epollHandler, EPOLL_CTL_DEL, session->sock(), nullptr)) {
        ERROR(ServerLogger) << "Can't remove " << session << " socket " << session->sock() << "error " << strerror(errno);
        throw std::make_error_code(std::errc(errno));
    }
    --m_activeSessions;
}

/*!
 * \brief SessionsEventLoop::deleteLater
 *
 * Postpones a session for deletion
 *
 * \param session to delete later
 */
void SessionsEventLoop::deleteLater(BasicServerSession *session) noexcept
{
    try {
        unregisterSession(session);
    } catch (...) {}
    // Idea "stolen" from Qt :)
    std::unique_lock<Dracon::SpinLock> lock{m_deleteLaterMutex};
    try {
        m_deleteLaterObjects.insert(session);
    } catch (...) {}
}

void SessionsEventLoop::shutdown() noexcept
{
    m_quit.store(true);
    eventfd_write(m_eventFd, 1);
}

std::shared_ptr<Dracon::CharBuffer> SessionsEventLoop::sharedWriteBuffer(size_t size) const
{
    if (m_loopThread.get_id() == std::this_thread::get_id() && size <= m_sharedWriteBuffer->size())
        return m_sharedWriteBuffer;
    return std::make_shared<Dracon::CharBuffer>(size);
}

void SessionsEventLoop::setWorkloadBalancing(bool on)
{
    m_workloadBalancing = on;
}

/*!
 * \brief SessionsEventLoop::sharedReadBuffer
 *
 * Quite useful buffer, used by all basic_server_session to temporary read all sock data.
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
    using Clock = std::chrono::high_resolution_clock;
    using TimePoint = std::chrono::time_point<Clock>;
    auto events = std::make_unique<epoll_event[]>(EventsSize);
    Ms timeout(-1ms); // Initial timeout
    while (!m_quit) {
        bool wokeup = false;
        TRACE(ServerLogger) << "timeout = " << timeout.count();
        int triggeredEvents = epoll_wait(m_epollHandler, events.get(), EventsSize, timeout.count());
        if (triggeredEvents < 0)
            continue;
        if (!m_workloadBalancing) {
            for (int i = 0 ; i < triggeredEvents; ++i) {
                auto &event = events[i];
                if (event.data.fd != m_eventFd)
                    reinterpret_cast<BasicServerSession *>(event.data.ptr)->processEvents(event.events);
                else
                    wokeup = true;
            }
        } else {
            std::vector<std::pair<BasicServerSession *, uint32_t>> sessionEvents;
            sessionEvents.reserve(triggeredEvents);
            for (int i = 0 ; i < triggeredEvents; ++i) {
                auto &event = events[i];
                if (event.data.fd != m_eventFd) {
                    auto ptr = reinterpret_cast<BasicServerSession *>(event.data.ptr);
                    auto evs = event.events;
                    std::pair<BasicServerSession *, uint32_t> ev{ptr, evs};
                    sessionEvents.insert(std::upper_bound(sessionEvents.begin(),
                                                          sessionEvents.end(),
                                                          ev,
                                                          [](auto a, auto b) {
                                                                return a.first->order() < b.first->order();
                                                          }),
                                         ev);
                } else {
                    wokeup = true;
                }
            }
            for (auto event : sessionEvents)
                event.first->processEvents(event.second);
        }

        std::unordered_set<BasicServerSession *> wokeup_sessions;
        if (wokeup) {
            uint64_t data;
            while(eventfd_read(m_eventFd, &data) == 0) {
                if (data != 1)
                    wokeup_sessions.insert(reinterpret_cast<BasicServerSession *>(data));
            }
        }
        // Some session(s) have timeout
        timeout = -1ms; // maximum timeout
        std::unique_lock<std::mutex> lock{m_sessionsMutex};
        std::vector<BasicServerSession *> sessions;
        sessions.reserve(m_sessions.size());
        for (auto session : m_sessions)
            sessions.push_back(session);
        lock.unlock(); // Allow the server to insert new connections

        auto now = Clock::now();
        for (auto session : sessions) {
            if (wokeup && wokeup_sessions.find(session) != wokeup_sessions.end())
                session->wakeup();
            auto session_timeout = session->nextTimeout();
            if (session_timeout == TimePoint{})
                continue;
            if (session_timeout <= now) {
                session->timeout();
            } else {// round to 1s
                auto next_timeout = std::max(1000ms, std::chrono::duration_cast<Ms>(session_timeout - now) + 50ms);
                timeout = timeout != -1ms ? std::min(timeout, next_timeout) : next_timeout;
            }
        }

        // Delete all deleteLater pending sessions
        std::unique_lock<Dracon::SpinLock> lock1{m_deleteLaterMutex};
        for (auto &obj : m_deleteLaterObjects)
            delete obj;
        m_deleteLaterObjects.clear();
    }
}

} // namespace Getodac
