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
#include <sys/eventfd.h>
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

sessions_event_loop::sessions_event_loop()
{
    unsigned long rmem_max = readProc("/proc/sys/net/core/rmem_max");

    // This buffer is shared by all basic_server_sessions which are server
    // by this event loop to read the incoming data without
    // allocating any memory
    shared_read_buffer.resize(rmem_max);
    m_shared_write_buffer = std::make_shared<dracon::char_buffer>(readProc("/proc/sys/net/core/wmem_max"));

    m_epoll_handler = epoll_create1(EPOLL_CLOEXEC);
    if (m_epoll_handler == -1)
        throw std::runtime_error{"Can't create epool handler"};

    m_event_fd = eventfd(0, EFD_NONBLOCK);
    epoll_event event;
    event.data.ptr = nullptr;
    event.data.fd = m_event_fd;
    event.events = EPOLLHUP | EPOLLERR | EPOLLIN | EPOLLET;
    if (epoll_ctl(m_epoll_handler, EPOLL_CTL_ADD, m_event_fd, &event))
        throw std::runtime_error{"Can't register the event handler"};

    m_loop_thread = std::thread([this]{loop();});

//    // set insane priority ?
//    sched_param sch;
//    sch.sched_priority = sched_get_priority_max(SCHED_RR);
//    pthread_setschedparam(m_loopThread.native_handle(), SCHED_RR, &sch);
    TRACE(server_logger) << this << " shared buffer mem_max: " << rmem_max << " eventfd = " << m_event_fd;
}

sessions_event_loop::~sessions_event_loop()
{
    shutdown();
    try {
        // Quit event loop
        m_loop_thread.join();

        // Destroy all registered sessions
        std::unique_lock<std::mutex> lock{m_sessions_mutex};
        auto it = m_sessions.begin();
        while (it != m_sessions.end()) {
            auto session = it++;
            lock.unlock();
            delete (*session);
            lock.lock();
        }
        close(m_event_fd);
    } catch (...) {}
    TRACE(server_logger) << this;
}

/*!
 * \brief SessionsEventLoop::registerSession
 *
 * Register a new basic_server_session to this even loop
 *
 * \param session to register
 * \param events the events that epoll will listen for
 */
void sessions_event_loop::register_session(basic_server_session *session, uint32_t events)
{
    TRACE(server_logger) << session << " events" << events << active_sessions();
    {
        std::unique_lock<std::mutex> lock{m_sessions_mutex};
        m_sessions.insert(session);
    }
    epoll_event event;
    event.data.ptr = session;
    event.events = events;
    if (epoll_ctl(m_epoll_handler, EPOLL_CTL_ADD, session->sock(), &event))
        throw std::runtime_error{"Can't register session"};
    ++m_active_sessions;
}

/*!
 * \brief SessionsEventLoop::updateSession
 *
 * Updates a registered basic_server_session
 *
 * \param session to update
 * \param events new epoll events
 */
void sessions_event_loop::update_session(basic_server_session *session, uint32_t events)
{
    TRACE(server_logger) << session << " events:" << events;
    epoll_event event;
    event.data.ptr = session;
    event.events = events;
    if (epoll_ctl(m_epoll_handler, EPOLL_CTL_MOD, session->sock(), &event))
        throw std::runtime_error{"Can't change the session"};
}

/*!
 * \brief SessionsEventLoop::unregisterSession
 *
 * \param session to unregister
 */
void sessions_event_loop::unregister_session(basic_server_session *session)
{
    TRACE(server_logger) << session << " activeSessions:" << active_sessions();
    {
        std::unique_lock<std::mutex> lock{m_sessions_mutex};
        if (!m_sessions.erase(session))
            return;
    }
    if (epoll_ctl(m_epoll_handler, EPOLL_CTL_DEL, session->sock(), nullptr)) {
        ERROR(server_logger) << "Can't remove " << session << " socket " << session->sock() << "error " << strerror(errno);
        throw std::make_error_code(std::errc(errno));
    }
    --m_active_sessions;
}

/*!
 * \brief SessionsEventLoop::deleteLater
 *
 * Postpones a session for deletion
 *
 * \param session to delete later
 */
void sessions_event_loop::delete_later(basic_server_session *session) noexcept
{
    unregister_session(session);
    // Idea "stolen" from Qt :)
    std::unique_lock<dracon::spin_lock> lock{m_deleteLater_mutex};
    try {
        m_delete_later_objects.insert(session);
    } catch (...) {}
}

void sessions_event_loop::shutdown() noexcept
{
    m_quit.store(true);
    eventfd_write(m_event_fd, 1);
}

std::shared_ptr<dracon::char_buffer> sessions_event_loop::shared_write_buffer(size_t size) const
{
    if (m_loop_thread.get_id() == std::this_thread::get_id() && size <= m_shared_write_buffer->size())
        return m_shared_write_buffer;
    return std::make_shared<dracon::char_buffer>(size);
}

void sessions_event_loop::setWorkloadBalancing(bool on)
{
    m_workload_balancing = on;
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


void sessions_event_loop::loop()
{
    using Ms = std::chrono::milliseconds;
    using Clock = std::chrono::high_resolution_clock;
    using TimePoint = std::chrono::time_point<Clock>;
    auto events = std::make_unique<epoll_event[]>(EventsSize);
    Ms timeout(-1ms); // Initial timeout
    while (!m_quit) {
        bool wokeup = false;
        TRACE(server_logger) << "timeout = " << timeout.count();
        int triggeredEvents = epoll_wait(m_epoll_handler, events.get(), EventsSize, timeout.count());
        if (triggeredEvents < 0)
            continue;
        if (!m_workload_balancing) {
            for (int i = 0 ; i < triggeredEvents; ++i) {
                auto &event = events[i];
                if (event.data.fd != m_event_fd)
                    reinterpret_cast<basic_server_session *>(event.data.ptr)->process_events(event.events);
                else
                    wokeup = true;
            }
        } else {
            std::vector<std::pair<basic_server_session *, uint32_t>> sessionEvents;
            sessionEvents.reserve(triggeredEvents);
            for (int i = 0 ; i < triggeredEvents; ++i) {
                auto &event = events[i];
                if (event.data.fd != m_event_fd) {
                    auto ptr = reinterpret_cast<basic_server_session *>(event.data.ptr);
                    auto evs = event.events;
                    std::pair<basic_server_session *, uint32_t> event{ptr, evs};
                    sessionEvents.insert(std::upper_bound(sessionEvents.begin(),
                                                          sessionEvents.end(),
                                                          event,
                                                          [](auto a, auto b) {
                                                                return a.first->order() < b.first->order();
                                                          }),
                                         event);
                } else {
                    wokeup = true;
                }
            }
            for (auto event : sessionEvents)
                event.first->process_events(event.second);
        }

        std::unordered_set<basic_server_session *> wokeup_sessions;
        if (wokeup) {
            uint64_t data;
            while(eventfd_read(m_event_fd, &data) == 0) {
                if (data != 1)
                    wokeup_sessions.insert(reinterpret_cast<basic_server_session *>(data));
            }
        }
        // Some session(s) have timeout
        timeout = -1ms; // maximum timeout
        std::unique_lock<std::mutex> lock{m_sessions_mutex};
        std::vector<basic_server_session *> sessions;
        sessions.reserve(m_sessions.size());
        for (auto session : m_sessions)
            sessions.push_back(session);
        lock.unlock(); // Allow the server to insert new connections

        auto now = Clock::now();
        for (auto session : sessions) {
            if (wokeup && wokeup_sessions.find(session) != wokeup_sessions.end())
                session->wake_up();
            auto session_timeout = session->next_timeout();
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
        std::unique_lock<dracon::spin_lock> lock1{m_deleteLater_mutex};
        for (auto &obj : m_delete_later_objects)
            delete obj;
        m_delete_later_objects.clear();
    }
}

} // namespace Getodac
