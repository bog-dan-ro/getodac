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

#ifndef SESSIONS_EVENT_LOOP_H
#define SESSIONS_EVENT_LOOP_H

#include <atomic>
#include <mutex>
#include <set>
#include <thread>
#include <unordered_set>
#include <vector>

#include <getodac/utils.h>

namespace Getodac {

class ServerSession;

/*!
 * \brief The SessionsEventLoop class
 *
 * This class is used by the server to handle the sessions (sock & timer) events.
 * Usually the server creates one event loop for every CPU core on the system.
 */
class SessionsEventLoop
{
public:
    SessionsEventLoop();
    ~SessionsEventLoop();

    void registerSession(ServerSession *session, uint32_t events);
    void updateSession(ServerSession *session, uint32_t events);
    void unregisterSession(ServerSession *session);

    void deleteLater(ServerSession *session) noexcept;

    inline int activeSessions() const noexcept { return m_activeSessions.load(); }
    inline void shutdown() noexcept { m_quit.store(true); }

    std::vector<char> sharedReadBuffer;

private:
    void loop();

private:
    int m_epollHandler;
    std::atomic_uint m_activeSessions{0};
    std::atomic_bool m_quit{false};
    std::thread m_loopThread;
    std::mutex m_sessionsMutex;
    std::set<ServerSession *> m_sessions;
    SpinLock m_deleteLaterMutex;
    std::unordered_set<ServerSession *> m_deleteLaterObjects;
};

} // namespace Getodac
#endif // SESSIONS_EVENT_LOOP_H
