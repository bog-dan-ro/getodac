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

#pragma once

#include <atomic>
#include <mutex>
#include <set>
#include <thread>
#include <unordered_set>
#include <vector>

#include <getodac/utils.h>

namespace Getodac {

class basic_server_session;

/*!
 * \brief The SessionsEventLoop class
 *
 * This class is used by the server to handle the sessions (sock & timer) events.
 * Usually the server creates one event loop for every CPU core on the system.
 */
class sessions_event_loop
{
public:
    sessions_event_loop();
    ~sessions_event_loop();

    void register_session(basic_server_session *session, uint32_t events);
    void update_session(basic_server_session *session, uint32_t events);
    void unregister_session(basic_server_session *session);

    void delete_later(basic_server_session *session) noexcept;

    inline int active_sessions() const noexcept { return m_active_sessions.load(); }
    void shutdown() noexcept;

    char_buffer shared_read_buffer;
    std::shared_ptr<char_buffer> shared_write_buffer(size_t size) const;
    void setWorkloadBalancing(bool on);

    inline int event_fd() const { return m_event_fd; }
private:
    void loop();

private:
    std::shared_ptr<char_buffer> m_shared_write_buffer;
    int m_epoll_handler;
    bool m_workload_balancing = false;
    int m_event_fd;
    std::atomic_uint m_active_sessions{0};
    std::atomic_bool m_quit{false};
    std::thread m_loop_thread;
    std::mutex m_sessions_mutex;
    std::set<basic_server_session *> m_sessions;
    spin_lock m_deleteLater_mutex;
    std::unordered_set<basic_server_session *> m_delete_later_objects;
};

} // namespace Getodac
