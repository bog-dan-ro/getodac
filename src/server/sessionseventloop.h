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

#pragma once

#include <atomic>
#include <mutex>
#include <set>
#include <thread>
#include <unordered_set>
#include <vector>

#include <dracon/utils.h>

namespace Getodac {

class BasicServerSession;

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

    void registerSession(BasicServerSession *session, uint32_t events);
    void updateSession(BasicServerSession *session, uint32_t events);
    void unregisterSession(BasicServerSession *session);

    void deleteLater(BasicServerSession *session) noexcept;

    inline uint32_t activeSessions() const noexcept { return m_activeSessions.load(); }
    void shutdown() noexcept;

    Dracon::CharBuffer sharedReadBuffer;
    std::shared_ptr<Dracon::CharBuffer> sharedWriteBuffer(size_t size) const;
    void setWorkloadBalancing(bool on);

    inline int eventFd() const { return m_eventFd; }
private:
    void loop();

private:
    std::shared_ptr<Dracon::CharBuffer> m_sharedWriteBuffer;
    int m_epollHandler;
    bool m_workloadBalancing = false;
    int m_eventFd;
    std::atomic<uint32_t> m_activeSessions{0};
    std::atomic_bool m_quit{false};
    std::thread m_loopThread;
    std::mutex m_sessionsMutex;
    std::set<BasicServerSession *> m_sessions;
    Dracon::SpinLock m_deleteLaterMutex;
    std::unordered_set<BasicServerSession *> m_deleteLaterObjects;
};

} // namespace Getodac
