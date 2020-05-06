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

#include "server_session.h"

#include <netinet/tcp.h>
#include <sys/epoll.h>

#include <cstring>
#include <functional>
#include <unordered_map>
#include <stdexcept>

#include <getodac/abstract_service_session.h>
#include <getodac/exceptions.h>
#include <getodac/logging.h>

#include "server.h"
#include "server_logger.h"

namespace Getodac
{

namespace {
    // https://www.w3.org/Protocols/rfc2616/rfc2616-sec10.html
    const std::unordered_map<unsigned, const char *> codes = {

        // Informational 1xx
        {100, "100 Continue\r\n"},
        {101, "101 Switching Protocols\r\n"},

        // Successful 2xx
        {200, "200 OK\r\n"},
        {201, "201 Created\r\n"},
        {202, "202 Accepted\r\n"},
        {203, "203 Non-Authoritative Information\r\n"},
        {204, "204 No Content\r\n"},
        {205, "205 Reset Content\r\n"},
        {206, "206 Partial Content\r\n"},

        // Redirection 3xx
        {300, "300 Multiple Choices\r\n"},
        {301, "301 Moved Permanently\r\n"},
        {302, "302 Found\r\n"},
        {303, "303 See Other\r\n"},
        {304, "304 Not Modified\r\n"},
        {305, "305 Use Proxy\r\n"},
        {306, "306 Switch Proxy\r\n"},
        {307, "307 Temporary Redirect\r\n"},

        // Client Error 4xx
        {400, "400 Bad Request\r\n"},
        {401, "401 Unauthorized\r\n"},
        {402, "402 Payment Required\r\n"},
        {403, "403 Forbidden\r\n"},
        {404, "404 Not Found\r\n"},
        {405, "405 Method Not Allowed\r\n"},
        {406, "406 Not Acceptable\r\n"},
        {407, "407 Proxy Authentication Required\r\n"},
        {408, "408 Request Timeout\r\n"},
        {409, "409 Conflict\r\n"},
        {410, "410 Gone\r\n"},
        {411, "411 Length Required\r\n"},
        {412, "412 Precondition Failed\r\n"},
        {413, "413 Request Entity Too Large\r\n"},
        {414, "414 Request-URI Too Long\r\n"},
        {415, "415 Unsupported Media Type\r\n"},
        {416, "415 Requested Range Not Satisfiable\r\n"},
        {417, "417 Expectation Failed\r\n"},

        // Server Error 5xx
        {500, "500 Internal Server Error\r\n"},
        {501, "501 Not Implemented\r\n"},
        {502, "502 Bad Gateway\r\n"},
        {503, "503 Service Unavailable\r\n"},
        {504, "504 Gateway Timeout\r\n"},
        {505, "505 HTTP Version Not Supported\r\n"}
    };

    const char ContinueResponse[] = "HTTP/1.1 100 Continue\r\n\r\n";

    inline const char *statusCode(unsigned status)
    {
        auto it = codes.find(status);
        if (it != codes.end())
            return it->second;

        return codes.at(500);
    }

    struct YieldImpl : AbstractServerSession::Yield
    {
        explicit YieldImpl(YieldType &yield)
            : m_yield(yield)
        {}
        inline void operator ()() override
        {
            m_yield();
        }

        inline AbstractServerSession::Action get() override
        {
            return m_yield.get();
        }
        YieldType &m_yield;
    };
}

ServerSession::ServerSession(SessionsEventLoop *eventLoop, int sock, const sockaddr_storage &sockAddr)
    : m_eventLoop(eventLoop)
    , m_sock(sock)
    , m_readResume(std::bind(&ServerSession::readLoop, this, std::placeholders::_1))
    , m_writeResume(std::bind(&ServerSession::writeLoop, this, std::placeholders::_1))
    , m_peerAddr(sockAddr)
{
    TRACE(Getodac::serverLogger) << "ServerSession::ServerSession " << (void*)this
                                 << " eventLoop: " << eventLoop
                                 << " socket:" << sock;
    int opt = 1;
    if (setsockopt(m_sock, SOL_TCP, TCP_NODELAY, &opt, sizeof(int)))
        throw std::runtime_error{"Can't set socket option TCP_NODELAY"};

    m_parser.data = this;
    http_parser_init(&m_parser, HTTP_REQUEST);
    setTimeout();
}

ServerSession::~ServerSession()
{
    quitRWLoops(Action::Quit);
    try {
        Server::instance()->serverSessionDeleted(this);
    } catch (const std::exception &e) {
        WARNING(serverLogger) << e.what();
    } catch (...) {}

    if (m_sock != -1) {
        try {
            m_eventLoop->unregisterSession(this);
        } catch (const std::exception &e) {
            WARNING(serverLogger) << e.what();
        }
        try {
            ::close(m_sock);
        } catch (...) {}
    }
    TRACE(Getodac::serverLogger) << "ServerSession::~ServerSession " << this;
}

ServerSession *ServerSession::sessionReady()
{
    m_eventLoop->registerSession(this, EPOLLIN | EPOLLPRI | EPOLLRDHUP | EPOLLET);
    return this;
}

void ServerSession::processEvents(uint32_t events) noexcept
{
    try {
        if (events & (EPOLLERR | EPOLLRDHUP | EPOLLHUP) ) {
            terminateSession(Action::Quit);
            return;
        }

        if (events & (EPOLLIN | EPOLLPRI)) {
            if (m_readResume) {
                m_readResume(Action::Continue);
                if (m_statusCode && m_statusCode != 200)
                    terminateSession(Action::Quit);
            } else {
                terminateSession(Action::Quit);
            }
            return;
        }

        if (events & EPOLLOUT) {
            if (m_writeResume)
                m_writeResume(Action::Continue);
            else
                terminateSession(Action::Quit);
        }
    } catch (const std::exception &e) {
        DEBUG(serverLogger) << e.what();
        m_eventLoop->deleteLater(this);
    } catch (...) {
        DEBUG(serverLogger) << "Unkown exception, terminating the session";
        m_eventLoop->deleteLater(this);
    }
}

void ServerSession::timeout() noexcept
{
    try {
        if (!m_statusCode) {
            m_statusCode = 408;
            m_tempStr.clear();
        }
        terminateSession(Action::Timeout);
    } catch (const std::exception &e) {
        DEBUG(serverLogger) << e.what();
    } catch (...) {}
}

void ServerSession::write(Yield &yield, const void *buf, size_t size)
{
    if (!m_resonseHeader.str().empty()) {
        std::string headers = m_resonseHeader.str();
        m_resonseHeader.str({});
        iovec vec[2];
        vec[0].iov_base = (void *)headers.c_str();
        vec[0].iov_len = headers.size();
        vec[1].iov_base = (void *)buf;
        vec[1].iov_len = size;
        writev(yield, (iovec *)&vec, 2);
        m_canWriteError = false;
        return;
    }
    while (yield.get() == Action::Continue) {
        auto written = sockWrite(buf, size);
        if (written < 0) {
            yield();
            continue;
        }
        if (size_t(written) == size)
            return;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpointer-arith"
        buf += written;
#pragma GCC diagnostic pop
        size -= written;
        yield();
    }
}

void ServerSession::writev(AbstractServerSession::Yield &yield, iovec *vec, size_t count)
{
    std::unique_ptr<iovec[]> _vec;
    std::string headers = m_resonseHeader.str();
    if (!headers.empty()) {
        m_resonseHeader.str({});
        _vec = std::make_unique<iovec[]>(count + 1);
        _vec[0].iov_base = (void *)headers.c_str();
        _vec[0].iov_len = headers.size();
        memcpy(&_vec[1], vec, sizeof(iovec) * count);
        vec = _vec.get();
        ++count;
    }

    while (yield.get() == Action::Continue) {
        auto written = sockWritev(vec, count);
        if (written < 0) {
            yield();
            continue;
        }

        for (size_t i = 0; i < count; ++i) {
            if (vec[i].iov_len <= size_t(written)) {
                written -= vec[i].iov_len;
            } else {
                vec = &vec[i];
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpointer-arith"
                vec->iov_base += written;
#pragma GCC diagnostic pop
                vec->iov_len -= written;
                written = 1; // written might be 0 at this point, but we still have things to write
                count -= i;
                break;
            }
        }

        if (!written)
            break;

        yield();
    }
}

void ServerSession::responseStatus(uint32_t code)
{
    m_resonseHeader.str({});
    m_resonseHeader << "HTTP/1.1 " << statusCode(m_statusCode = code);
}

void ServerSession::responseHeader(const std::string &field, const std::string &value)
{
    m_resonseHeader << field << ": " << value << crlf;
}

void ServerSession::responseEndHeader(uint64_t contentLenght, uint32_t keepAliveSeconds, bool continousWrite)
{
    if (contentLenght == ChunkedData)
        m_resonseHeader << "Transfer-Encoding: chunked\r\n";
    else
        m_resonseHeader << "Content-Length: " << contentLenght << crlf;

    if (http_should_keep_alive(&m_parser) && keepAliveSeconds) {
        m_keepAliveSeconds = keepAliveSeconds;
        m_resonseHeader << "Keep-Alive: timeout=" << keepAliveSeconds << crlf;
        m_resonseHeader << "Connection: keep-alive\r\n";
    } else {
        m_resonseHeader << "Connection: close\r\n";
    }
    m_resonseHeader << crlf;

    if (!contentLenght) {
        std::string headers = m_resonseHeader.str();
        sockWrite(headers.c_str(), headers.size());
        m_canWriteError = false;
    } else {
        // Switch to write mode
        uint32_t events = EPOLLOUT | EPOLLRDHUP;
        if (continousWrite)
            events |= EPOLLET;

        m_eventLoop->updateSession(this, events);
    }
}

void ServerSession::responseComplete()
{
    // switch to read mode
    m_statusCode = 0;
    m_eventLoop->updateSession(this, EPOLLIN | EPOLLPRI | EPOLLRDHUP | EPOLLET);
    if (m_keepAliveSeconds)
        setTimeout(std::chrono::seconds(m_keepAliveSeconds));
    else
        terminateSession(Action::Quit);
    Server::instance()->sessionServed();
}

int ServerSession::sendBufferSize() const
{
    int optval = 0;
    socklen_t optlen = sizeof(optval);
    getsockopt(m_sock, SOL_SOCKET, SO_SNDBUF, &optval, &optlen);
    return optval;
}

bool ServerSession::setSendBufferSize(int size)
{
    return 0 == setsockopt(m_sock, SOL_SOCKET, SO_SNDBUF, &size, sizeof(int));
}

int ServerSession::receiveBufferSize() const
{
    int optval = 0;
    socklen_t optlen = sizeof(optval);
    getsockopt(m_sock, SOL_SOCKET, SO_RCVBUF, &optval, &optlen);
    return optval;
}

bool ServerSession::setReceiveBufferSize(int size)
{
    return 0 == setsockopt(m_sock, SOL_SOCKET, SO_RCVBUF, &size, sizeof(int));
}

void ServerSession::readLoop(YieldType &yield)
{
    http_parser_settings settings;
    memset(&settings, 0, sizeof(settings));

    // we set only the fields fields used in a request
    settings.on_message_begin = &ServerSession::messageBegin;
    settings.on_url = &ServerSession::url;
    settings.on_header_field = &ServerSession::headerField;
    settings.on_header_value = &ServerSession::headerValue;
    settings.on_headers_complete = &ServerSession::headersComplete;
    settings.on_body = &ServerSession::body;
    settings.on_message_complete = &ServerSession::messageComplete;
    std::vector<char> tempBuffer;
    while (yield.get() == Action::Continue) {
        try {
            setTimeout();
            auto tempSize = tempBuffer.size();
            auto sz = sockRead(m_eventLoop->sharedReadBuffer.data() + tempSize, m_eventLoop->sharedReadBuffer.size() - tempSize);

            if (sz <= 0) {
                yield();
                continue;
            }

            if (tempSize)
                memcpy(m_eventLoop->sharedReadBuffer.data(), tempBuffer.data(), tempSize);

            auto parsedBytes = http_parser_execute(&m_parser, &settings, m_eventLoop->sharedReadBuffer.data(), tempSize + sz);
            if (m_parser.http_errno)
                return;
            tempBuffer.clear();
            if (size_t(sz) > parsedBytes) {
                auto tempLen = sz - parsedBytes;
                tempBuffer.resize(tempLen);
                memcpy(tempBuffer.data(), m_eventLoop->sharedReadBuffer.data() + parsedBytes, tempLen);
            }
#ifndef ENABLE_STRESS_TEST
            yield();
#endif
        } catch (const std::exception &e) {
            DEBUG(serverLogger) << e.what();
            m_eventLoop->deleteLater(this);
        } catch (...) {
            m_eventLoop->deleteLater(this);
        }
    }
}

void ServerSession::writeLoop(YieldType &yield)
{
    YieldImpl yi{yield};
    while (yield.get() == Action::Continue) {
        try {
            if (m_serviceSession)
                m_serviceSession->writeResponse(yi);
            setTimeout();
            yield();
        } catch (const std::exception &e) {
            DEBUG(serverLogger) << e.what();
            m_serviceSession.reset();
            m_eventLoop->deleteLater(this);
        } catch (...) {
            m_serviceSession.reset();
            m_eventLoop->deleteLater(this);
        }
    }
}

void ServerSession::terminateSession(Action action)
{
    quitRWLoops(action);
    if (m_canWriteError && m_statusCode && m_statusCode != 200) {
        try {
            responseStatus(m_statusCode);
            for (const auto &kv : m_responseStatusErrorHeaders)
                responseHeader(kv.first, kv.second);
            auto contentLength = m_tempStr.size();
            responseEndHeader(contentLength, 0);
            if (contentLength) {
                // responseEndHeader doesn't write the headers immediately
                // if there is some data to send
                m_resonseHeader << m_tempStr;
                auto str = m_resonseHeader.str();
                sockWrite(str.c_str(), str.size());
            }
            m_statusCode = 0;
            m_resonseHeader.str({});
            m_tempStr.clear();
            m_canWriteError = false;
        } catch (const std::exception &e) {
            DEBUG(serverLogger) << e.what();
        } catch (...) { }
    }

    if (sockShutdown())
        m_eventLoop->deleteLater(this);
    else
        setTimeout(50ms);
}

void ServerSession::setTimeout(const std::chrono::milliseconds &ms)
{
    if (ms == std::chrono::milliseconds::zero())
        m_nextTimeout = TimePoint();
    else
        m_nextTimeout = std::chrono::high_resolution_clock::now() + ms;
}

int ServerSession::messageBegin(http_parser *parser)
{
    auto thiz = reinterpret_cast<ServerSession *>(parser->data);
    thiz->m_tempStr.clear();
    thiz->m_headerField.clear();
    thiz->m_contentLength = ChunkedData;
    thiz->m_parserStatus = HttpParserStatus::Url;
    return 0;
}

int ServerSession::url(http_parser *parser, const char *at, size_t length)
{
    auto thiz = reinterpret_cast<ServerSession *>(parser->data);
    if (thiz->m_parserStatus != HttpParserStatus::Url) {
        int res = thiz->httpParserStatusChanged(parser);
        if (res)
            return res;
        thiz->m_parserStatus = HttpParserStatus::Url;
        thiz->m_headerField.clear();
        thiz->m_tempStr.clear();
    }
    try {
        thiz->m_canWriteError = true;
        thiz->m_tempStr.append(std::string{at, length});
    } catch (const ResponseStatusError &status) {
        return thiz->setResponseStatusError(status);
    } catch (...) {
        return (thiz->m_statusCode = 500); // Internal Server error
    }

    return 0;
}

int ServerSession::headerField(http_parser *parser, const char *at, size_t length)
{
    auto thiz = reinterpret_cast<ServerSession *>(parser->data);
    if (thiz->m_parserStatus != HttpParserStatus::HeaderField) {
        int res = thiz->httpParserStatusChanged(parser);
        if (res)
            return res;
        thiz->m_parserStatus = HttpParserStatus::HeaderField;
        thiz->m_headerField.clear();
        thiz->m_tempStr.clear();
    }
    try {
        thiz->m_headerField.append(std::string{at, length});
    } catch (...) {
        thiz->m_headerField.clear();
        return (thiz->m_statusCode = 500); // Internal Server error
    }
    return 0;
}

int ServerSession::headerValue(http_parser *parser, const char *at, size_t length)
{
    auto thiz = reinterpret_cast<ServerSession *>(parser->data);
    if (thiz->m_parserStatus != HttpParserStatus::HeaderValue) {
        int res = thiz->httpParserStatusChanged(parser);
        if (res)
            return res;
        thiz->m_parserStatus = HttpParserStatus::HeaderValue;
        thiz->m_tempStr.clear();
    }
    try {
        thiz->m_tempStr.append(std::string{at, length});
    } catch (const ResponseStatusError &status) {
        return thiz->setResponseStatusError(status);
    } catch (...) {
        return (thiz->m_statusCode = 500); // Internal Server error
    }
    return 0;
}

int ServerSession::headersComplete(http_parser *parser)
{
    auto thiz = reinterpret_cast<ServerSession *>(parser->data);
    if (thiz->m_parserStatus == HttpParserStatus::HeaderField) {
        thiz->m_tempStr.clear();
        thiz->m_parserStatus = HttpParserStatus::HeaderValue;
    }
    int res = thiz->httpParserStatusChanged(parser);
    if (res)
        return res;
    try {
        thiz->m_serviceSession->headersComplete();
    } catch (const ResponseStatusError &status) {
        return thiz->setResponseStatusError(status);
    } catch (...) {
        return (thiz->m_statusCode = 500); // Internal Server error
    }
    return 0;
}

int ServerSession::body(http_parser *parser, const char *at, size_t length)
{
    auto thiz = reinterpret_cast<ServerSession *>(parser->data);
    try {
        thiz->m_serviceSession->body(at, length);
    } catch (const ResponseStatusError &status) {
        return thiz->setResponseStatusError(status);
    } catch (...) {
        return (thiz->m_statusCode = 500); // Internal Server error
    }
    return 0;
}

int ServerSession::messageComplete(http_parser *parser)
{
    auto thiz = reinterpret_cast<ServerSession *>(parser->data);
    try {
        thiz->messageComplete();
        thiz->m_serviceSession->requestComplete();
    } catch (const ResponseStatusError &status) {
        return thiz->setResponseStatusError(status);
    } catch (...) {
        return (thiz->m_statusCode = 500); // Internal Server error
    }
    return 0;
}

int ServerSession::httpParserStatusChanged(http_parser *parser)
{
    try {
        switch (m_parserStatus) {
        case HttpParserStatus::Url:
            m_serviceSession = Server::instance()->createServiceSession(this, m_tempStr,
                                                                        http_method_str(http_method(parser->method)));
            if (!m_serviceSession)
                return (m_statusCode = 503); // Service Unavailable
            break;
        case HttpParserStatus::HeaderField:
            break; // Do nothing
        case HttpParserStatus::HeaderValue:
            if (m_headerField == "Content-Length") {
                char *end;
                m_contentLength = std::strtoul(m_tempStr.c_str(), &end, 10);
                if (end != m_tempStr.c_str() + m_tempStr.size())
                    m_contentLength = ChunkedData;
            } else if (m_headerField == "Expect" && m_tempStr.size() > 3 && std::memcmp(m_tempStr.c_str(), "100", 3) == 0) {
                if (m_contentLength == ChunkedData || !m_serviceSession->acceptContentLength(m_contentLength)) {
                    m_tempStr.clear();
                    m_headerField.clear();
                    return (m_statusCode = 417); // Expectation Failed
                } else {
                    sockWrite(ContinueResponse, sizeof(ContinueResponse));
                }
            }
            m_serviceSession->headerFieldValue(m_headerField, m_tempStr);
            m_tempStr.clear();
            m_headerField.clear();
            break;
        }
    } catch (const ResponseStatusError &status) {
        return setResponseStatusError(status);
    } catch (...) {
        return (m_statusCode = 500); // Internal Server error
    }
    return 0;
}

int ServerSession::setResponseStatusError(const ResponseStatusError &status)
{
    m_responseStatusErrorHeaders = status.headers();
    m_tempStr = status.what();
    return (m_statusCode = status.statusCode());
}

} // namespace Getodac
