/*
    Copyright (C) 2016, BogDan Vatra <bogdan@kde.org>

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

#ifndef SERVER_H
#define SERVER_H

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <string>
#include <unordered_set>
#include <vector>

#include <openssl/ssl.h>

#include <getodac/abstract_server_session.h>
#include <getodac/utils.h>

#include "server_plugin.h"

namespace Getodac {

class ServerSession;
class AbstractServiceSession;
class SessionsEventLoop;

class Server
{
public:
    static Server *instance();
    int exec(int argc, char *argv[]);
    void serverSessionDeleted(ServerSession *session);
    std::shared_ptr<AbstractServiceSession> createServiceSession(ServerSession *serverSession, const std::string &url, const std::string &method);
    uint32_t peakSessions();
    uint32_t activeSessions();
    std::chrono::seconds uptime() const;
    inline void sessionServed() { ++m_servedSessions; }
    uint64_t servedSessions() const { return m_servedSessions; }
    SSL_CTX *sslContext() const;
    static int SSLDataIndex;

private:
    Server();
    ~Server();

    static void exitSignalHandler(int);
    enum SocketType {
        IPV4,
        IPV6
    };
    int bind(SocketType type, int port);

private:
    std::atomic_bool m_shutdown;
    std::atomic<uint32_t> m_peakSessions;
    std::atomic<uint64_t> m_servedSessions;
    int m_eventsSize = 0;
    int m_epollHandler;
    SpinLock m_activeSessionsMutex;
    std::unordered_set<ServerSession*> m_activeSessions;
    std::vector<ServerPlugin> m_plugins;
    std::chrono::system_clock::time_point m_startTime;
    SSL_CTX *m_SSLContext = nullptr;
    int https4Sock = -1;
    int https6Sock = -1;
};

} // namespace Getodac

#endif // SERVER_H
