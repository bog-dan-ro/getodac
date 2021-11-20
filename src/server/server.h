/*
    Copyright (C) 2021, BogDan Vatra <bogdan@kde.org>

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

#include <netdb.h>

#include <atomic>
#include <condition_variable>
#include <map>
#include <mutex>
#include <string>
#include <unordered_set>
#include <vector>

#include <openssl/ssl.h>

#include <dracon/logging.h>
#include <dracon/utils.h>

#include "serverplugin.h"

namespace Dracon {
class AbstractStream;
class Request;
}

namespace Getodac {

class BasicServerSession;
class SessionsEventLoop;

class Server
{
public:
    static Server &instance();
    int exec(int argc, char *argv[]);
    void serverSessionCreated(BasicServerSession *session);
    void serverSessionDeleted(BasicServerSession *session);
    std::function<void(Dracon::AbstractStream &, Dracon::Request &)> create_session(const Dracon::Request &request);
    size_t peakSessions() const;
    size_t activeSessions() const;
    std::chrono::seconds uptime() const;
    inline void sessionServed() { ++m_servedSessions; }
    uint64_t servedSessions() const { return m_servedSessions; }
    SSL_CTX *sslContext() const;
    static void exitSignalHandler();

private:
    Server();
    ~Server();

    enum SocketType {
        IPV4,
        IPV6
    };
    int bind(SocketType type, int port);

private:
    std::atomic_bool m_shutdown{false};
    std::atomic<size_t> m_peakSessions{0};
    std::atomic<size_t> m_servedSessions{0};
    int m_eventsSize = 0;
    int m_epollHandler;
    mutable std::mutex m_activeSessionsMutex;
    std::unordered_set<BasicServerSession*> m_activeSessions;
    std::vector<ServerPlugin> m_plugins;
    std::chrono::system_clock::time_point m_startTime;
    SSL_CTX *m_sslContext = nullptr;
    std::mutex m_connectionsPerIpMutex;
    std::map<std::string, uint32_t> m_connectionsPerIp;
    int m_https4Sock = -1;
    int m_https6Sock = -1;
};

} // namespace Getodac
