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

#ifndef SERVER_SESSION_H
#define SERVER_SESSION_H

#include "sessions_event_loop.h"

#include "http_parser.h"

#include <unistd.h>
#include <sys/socket.h>

#include <chrono>
#include <memory>
#include <mutex>
#include <sstream>

#include <boost/coroutine2/coroutine.hpp>

#include <getodac/abstract_server_session.h>
#include <getodac/abstract_service_session.h>

using namespace std::chrono_literals;

namespace Getodac {

/*!
 * \brief The ServerSession class
 *
 * This class servers HTTP requests
 */
using YieldType = boost::coroutines2::coroutine<AbstractServerSession::Action>::pull_type;
class ServerSession : public AbstractServerSession
{
    using Clock = std::chrono::high_resolution_clock;
    using TimePoint = std::chrono::time_point<Clock>;

public:
    ServerSession(SessionsEventLoop *eventLoop, int sock, const sockaddr_storage &sockAddr);
    ~ServerSession() override;

    ServerSession *sessionReady();

    inline int sock() const { return m_sock;}
    inline const TimePoint & nextTimeout() { return m_nextTimeout; }

    void processEvents(uint32_t events) noexcept;
    void timeout() noexcept;

    virtual ssize_t sockRead(void  *buf, size_t size)
    {
        std::unique_lock<std::mutex> lock{m_sockMutex};
        if (m_sock == -1)
            throw std::logic_error("Socket is closed");
        return ::read(m_sock, buf, size);
    }

    virtual ssize_t sockWrite(const void  *buf, size_t size)
    {
        std::unique_lock<std::mutex> lock{m_sockMutex};
        if (m_sock == -1)
            throw std::logic_error("Socket is closed");
        auto ret = ::write(m_sock, buf, size);
        if (ret < 0 && errno == EPIPE)
            throw std::logic_error("Socket is closed");
        return ret;
    }

    virtual ssize_t sockWritev(const struct iovec *vec, int count)
    {
        std::unique_lock<std::mutex> lock{m_sockMutex};
        if (m_sock == -1)
            throw std::logic_error("Socket is closed");
        auto ret = ::writev(m_sock, vec, count);
        if (ret < 0 && errno == EPIPE)
            throw std::logic_error("Socket is closed");
        return ret;
    }

    virtual bool sockShutdown()
    {
        if (m_wasShutdown)
            return true;
        m_wasShutdown = true;
        return 0 == ::shutdown(m_sock, SHUT_RDWR);
    }

    // AbstractServerSession interface
    inline const struct sockaddr_storage& peerAddress() const override { return m_peerAddr; }
    void write(Yield &yield, const void *buf, size_t size) override;
    void writev(Yield &yield, iovec *vec, size_t count) override;
    void responseStatus(uint32_t code) override;
    void responseHeader(const std::string &field, const std::string &value) override;
    void responseEndHeader(uint64_t contentLenght, uint32_t keepAliveSeconds = 10, bool continousWrite = false) override;
    void responseComplete() override;
    int sendBufferSize() const override;
    bool setSendBufferSize(int size) override;
    int receiveBufferSize() const override;
    bool setReceiveBufferSize(int size) override;

protected:
    virtual void readLoop(YieldType &yield);
    void writeLoop(YieldType &yield);
    inline void quitRWLoops(Action action)
    {
        while (m_readResume) // Quit read loop
            m_readResume(action);
        while (m_writeResume) // Quit write loop
            m_writeResume(action);
    }
    void terminateSession(Action action);
    void setTimeout(const std::chrono::milliseconds &ms = 5s);

    static int messageBegin(http_parser *parser);
    static int url(http_parser *parser, const char *at, size_t length);
    static int headerField(http_parser *parser, const char *at, size_t length);
    static int headerValue(http_parser *parser, const char *at, size_t length);
    static int headersComplete(http_parser *parser);
    static int body(http_parser *parser, const char *at, size_t length);
    static int messageComplete(http_parser *parser);
    virtual inline void messageComplete() {}
    int setResponseStatusError(const ResponseStatusError &status);

private:
    SessionsEventLoop *m_eventLoop;
    int m_sock;
    std::mutex m_sockMutex;
    TimePoint m_nextTimeout;

    typedef boost::coroutines2::coroutine<Action>::push_type Call;
    Call m_readResume;
    Call m_writeResume;
    uint32_t m_statusCode = 0;
    http_parser m_parser;
    std::string m_tempStr;
    std::shared_ptr<AbstractServiceSession> m_serviceSession;
    std::ostringstream m_resonseHeader;
    uint32_t m_keepAliveSeconds = 10;
    struct sockaddr_storage m_peerAddr;
    bool m_canWriteError = true;
    bool m_wasShutdown = false;
    std::unordered_map<std::string, std::string> m_responseStatusErrorHeaders;
};

} // namespace Getodac

#endif // SERVER_SESSION_H
