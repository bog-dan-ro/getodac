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

    AGPL EXCEPTION:
    The AGPL license applies only to this file itself.

    As a special exception, the copyright holders of this file give you permission
    to use it, regardless of the license terms of your work, and to copy and distribute
    them under terms of your choice.
    If you do any changes to this file, these changes must be published under AGPL.
*/

#include "streams.h"

#include <sys/uio.h>

#include <dracon/http.h>
#include <dracon/logging.h>

#include "server.h"
#include "serverlogger.h"
#include "sessionseventloop.h"


namespace Getodac {

template<typename T>
using deleted_unique_ptr = std::unique_ptr<T,std::function<void(T*)>>;

using namespace std::chrono_literals;

struct http_parser_data
{
    Dracon::Request &req;
    std::string header_field;
};


BasicHttpSession::BasicHttpSession(SessionsEventLoop *eventLoop, int socket, YieldType &yield, const sockaddr_storage &peer_address, const std::shared_ptr<AbstractWakeupper> &wakeupper)
    : m_yield(yield)
    , m_socket(socket)
    , m_peerAddress(peer_address)
    , m_eventLoop(eventLoop)
    , m_wakeupper(wakeupper)
{
    memset(&m_settings, 0, sizeof(m_settings));
    m_settings.on_message_begin = &BasicHttpSession::messageBegin;
    m_settings.on_url = &BasicHttpSession::url;
    m_settings.on_header_field = &BasicHttpSession::headerField;
    m_settings.on_header_value = &BasicHttpSession::headerValue;
    m_settings.on_headers_complete = &BasicHttpSession::headersComplete;
    m_settings.on_body = &BasicHttpSession::body;
    m_settings.on_message_complete = &BasicHttpSession::messageComplete;
    http_parser_init(&m_parser, HTTP_REQUEST);
    sessionTimeout(5s);
}

BasicHttpSession::~BasicHttpSession() = default;

void BasicHttpSession::read(Dracon::Request &req) noexcept(false)
{
    if (req.State() == Dracon::Request::State::Completed)
        return;

    m_can_write_errror = true;
    auto &buffer = m_eventLoop->shared_read_buffer;
    http_parser_data data{.req = req};
    m_parser.data = &data;
    if (m_httpParserBuffer.currentSize()) {
        auto parsed_bytes = http_parser_execute(&m_parser, &m_settings, m_httpParserBuffer.currentData(), m_httpParserBuffer.currentSize());
        if (m_parser.http_errno) {
            INFO(Getodac::ServerLogger) << Dracon::addressText(peerAddress()) << " http parser error " << http_errno_name(http_errno(m_parser.http_errno));
            throw std::make_error_code(std::errc::bad_message);
        }
        m_httpParserBuffer.advance(parsed_bytes);
    }
    while (req.State() != Dracon::Request::State::Completed) {
        buffer.reset();
        std::error_code ec;
        auto temp_size = m_httpParserBuffer.currentSize();
        if (temp_size)
            buffer *= m_httpParserBuffer.currentString();
        auto sz = readSome({buffer.currentData(), buffer.currentSize()}, ec);
        if (ec)
            throw ec;
        if (!sz) {
            if ((ec = m_yield().get()))
                throw ec;
            continue;
        }
        buffer.reset();
        buffer.setCurrentSize(sz + temp_size);
        auto parsed_bytes = http_parser_execute(&m_parser, &m_settings,
                                                buffer.currentData(),
                                                buffer.currentSize());
        if (m_parser.http_errno) {
            INFO(Getodac::ServerLogger) << Dracon::addressText(peerAddress()) << " http parser error " << http_errno_name(http_errno(m_parser.http_errno));
            throw std::make_error_code(std::errc::bad_message);
        }
        if (req.State() == Dracon::Request::State::Completed)
            break;
        buffer.advance(parsed_bytes);
        if (buffer.currentSize())
            m_httpParserBuffer = buffer.currentString();
        else
            m_httpParserBuffer.clear();
    }
    m_httpParserBuffer.clear();
}

void BasicHttpSession::write(Dracon::ConstBuffer buffer) noexcept(false)
{
    while (buffer.length) {
        std::error_code ec;
        size_t written = writeSome(buffer, ec);
        if (!written) {
            if ((ec = m_yield().get()))
                throw ec;
            continue;
        }
        if (ec)
            throw ec;
        buffer.c_ptr += written;
        buffer.length -= written;
    }
}

void BasicHttpSession::write(std::vector<Dracon::ConstBuffer> buffers) noexcept(false)
{
    for(;;) {
        std::error_code ec;
        auto written = writeSome(buffers, ec);
        if (ec)
            throw ec;
        if (!written) {
            if ((ec = m_yield().get()))
                throw ec;
            continue;
        }

        for (size_t i = 0; i < buffers.size(); ++i) {
            if (buffers[i].length <= size_t(written)) {
                written -= buffers[i].length;
            } else {
                // TODO use std::span
                auto tmp = std::vector<Dracon::ConstBuffer>(buffers.begin() + i, buffers.end());
                buffers = std::move(tmp);
                buffers[0].c_ptr += written;
                buffers[0].length -= written;
                break;
            }
        }

        if (!written)
            break;
    }
}

std::error_code Getodac::BasicHttpSession::yield() noexcept
{
    return m_yield().get();
}

std::shared_ptr<Dracon::AbstractStream::AbstractWakeupper> BasicHttpSession::wakeupper() const noexcept
{
    return m_wakeupper;
}

void BasicHttpSession::keepAlive(std::chrono::seconds seconds) noexcept
{
    m_keepAlive = seconds;
}

std::chrono::seconds BasicHttpSession::keepAlive() const noexcept
{
    return m_keepAlive;
}

const sockaddr_storage &BasicHttpSession::peerAddress() const noexcept
{
    return m_peerAddress;
}

int BasicHttpSession::socketWriteSize() const noexcept(false)
{
    int optval = 0;
    socklen_t optlen = sizeof(optval);
    getsockopt(m_socket, SOL_SOCKET, SO_SNDBUF, &optval, &optlen);
    return optval / 2;
}

void BasicHttpSession::socketWriteSize(int size) noexcept(false)
{
    if (setsockopt(m_socket, SOL_SOCKET, SO_SNDBUF, &size, sizeof(int)))
        throw std::make_error_code(std::errc::invalid_argument);
}

int BasicHttpSession::socketReadSize() const noexcept(false)
{
    int optval = 0;
    socklen_t optlen = sizeof(optval);
    getsockopt(m_socket, SOL_SOCKET, SO_RCVBUF, &optval, &optlen);
    return optval / 2;
}

void BasicHttpSession::socketReadSize(int size) noexcept(false)
{
    if (setsockopt(m_socket, SOL_SOCKET, SO_RCVBUF, &size, sizeof(int)))
        throw std::make_error_code(std::errc::invalid_argument);
}

std::chrono::seconds BasicHttpSession::sessionTimeout() const noexcept
{
    return m_sessionTimeout;
}

void BasicHttpSession::sessionTimeout(std::chrono::seconds seconds) noexcept
{
    m_sessionTimeout = seconds;
    if (seconds.count())
        m_sessionTimeoutTimePoint = std::chrono::high_resolution_clock::now() + seconds;
    else
        m_sessionTimeoutTimePoint = {};
}

TimePoint BasicHttpSession::nextTimeout() const noexcept
{
    return m_sessionTimeoutTimePoint;
}

void BasicHttpSession::ioLoop()
{
    try {
        sessionTimeout(5s); // 5 seconds to read the headers
        do {
            Dracon::Request req = readHeaders();
            keepAlive(req.keepAlive() * 10s);
            auto session = Server::instance()->create_session(req);
            if (!session) {
                INFO(Getodac::ServerLogger) << Dracon::addressText(peerAddress()) << " invalid url " << req.method() << " " << req.url();
                write(Dracon::Response{503}.toString());
                break;
            }
            size_t content_length = req.contentLength();
            if (content_length != Dracon::ChunkedData)
                sessionTimeout(10s + 1s* (content_length / (512 * 1024)));
            else
                sessionTimeout(5min); // In this case the session should set a proper timeout
            session(*this, req);
            sessionTimeout(keepAlive());
            Server::instance()->sessionServed();
        } while (!m_yield.get() && m_keepAlive.count());
    } catch (int error) {
        INFO(Getodac::ServerLogger) << Dracon::addressText(peerAddress()) << " status code " << error;
        if (m_can_write_errror) {
            write(Dracon::Response{error > 0 && error < std::numeric_limits<uint16_t>::max()
                           ? uint16_t(error)
                           : uint16_t(500)}.toString());
        }
    } catch (const Dracon::Response &res) {
        INFO(Getodac::ServerLogger) << Dracon::addressText(peerAddress()) << " status code " << res.statusCode() << " body " << res.body();
        if (m_can_write_errror)
            write(res.toString());
    } catch (const std::exception &e) {
        INFO(Getodac::ServerLogger) << Dracon::addressText(peerAddress()) << " std message " << e.what();
        if (m_can_write_errror)
            write(Dracon::Response{500, e.what()}.toString());
    } catch (const std::error_code &ec) {
        INFO(Getodac::ServerLogger) <<  Dracon::addressText(peerAddress()) << " error code " << ec.message();
        if (m_can_write_errror)
            write(Dracon::Response{500, ec.message()}.toString());
    } catch (...) {
        INFO(Getodac::ServerLogger) << Dracon::addressText(peerAddress()) << " Unknown error";
        if (m_can_write_errror)
            write(Dracon::Response{}.toString());
    }
    shutdown();
}

int BasicHttpSession::messageBegin(http_parser *parser)
{
    auto data = reinterpret_cast<http_parser_data*>(parser->data);
    data->req.State(Dracon::Request::State::ProcessingUrl);
    return 0;
}

int BasicHttpSession::url(http_parser *parser, const char *at, size_t length)
{
    auto data = reinterpret_cast<http_parser_data*>(parser->data);
    data->req.url(std::string{at, length});
    data->req.method(http_method_str(http_method(parser->method)));
    data->req.State(Dracon::Request::State::ProcessingHeader);
    return 0;
}

int BasicHttpSession::headerField(http_parser *parser, const char *at, size_t length)
{
    auto data = reinterpret_cast<http_parser_data*>(parser->data);
    data->header_field = std::string{at, length};
    return 0;
}

int BasicHttpSession::headerValue(http_parser *parser, const char *at, size_t length)
{
    auto data = reinterpret_cast<http_parser_data*>(parser->data);
    data->req[std::move(data->header_field)] = std::string{at, length};
    return 0;
}

int BasicHttpSession::headersComplete(http_parser *parser)
{
    auto data = reinterpret_cast<http_parser_data*>(parser->data);
    data->req.State(Dracon::Request::State::HeadersCompleted);
    data->req.keepAlive(http_should_keep_alive(parser));
    return 0;
}

int BasicHttpSession::body(http_parser *parser, const char *at, size_t length)
{
    auto data = reinterpret_cast<http_parser_data*>(parser->data);
    data->req.appendBody({at, length});
    return 0;
}

int BasicHttpSession::messageComplete(http_parser *parser)
{
    auto data = reinterpret_cast<http_parser_data*>(parser->data);
    data->req.State(Dracon::Request::State::Completed);
    return 0;
}

Dracon::Request BasicHttpSession::readHeaders()
{
    Dracon::Request req;
    http_parser_data data{.req = req};

    m_parser.data = &data;
    http_parser_init(&m_parser, HTTP_REQUEST);

    auto &buffer = m_eventLoop->shared_read_buffer;
    while(req.State() != Dracon::Request::State::HeadersCompleted &&
        req.State() != Dracon::Request::State::Completed) {
        buffer.reset();
        std::error_code ec;
        auto temp_size = m_httpParserBuffer.currentSize();
        if (temp_size)
            buffer *= m_httpParserBuffer.currentData();
        auto sz = readSome({buffer.currentData(), buffer.currentSize()}, ec);
        buffer.setCurrentData(0);
        if (ec)
            throw ec;
        if (!sz) {
            if ((ec = m_yield().get()))
                throw ec;
            continue;
        }
        m_can_write_errror = true;
        buffer.reset();
        buffer.setCurrentSize(sz + temp_size);
        while ((req.State() != Dracon::Request::State::HeadersCompleted &&
                req.State() != Dracon::Request::State::Completed) &&
               buffer.currentSize()) {
            // find next crlf
            auto next = buffer.currentString().find(Dracon::CrlfString);
            if (next == std::string::npos)
                next = buffer.currentSize();
            else
                next += 2;
            auto parsed_bytes = http_parser_execute(&m_parser, &m_settings, buffer.currentData(), next);
            if (m_parser.http_errno) {
                INFO(Getodac::ServerLogger) << Dracon::addressText(peerAddress()) << " http parser error " << http_errno_name(http_errno(m_parser.http_errno));
                throw std::make_error_code(std::errc::bad_message);
            }
            buffer.advance(next);
            if (next != parsed_bytes)
                break;
        }
        if (buffer.currentSize())
            m_httpParserBuffer = buffer.currentString();
        else
            m_httpParserBuffer.clear();
    }
    return req;
}


SocketSession::SocketSession(SessionsEventLoop *eventLoop, int socket, YieldType &yield, const sockaddr_storage &peer_address, const std::shared_ptr<AbstractWakeupper> &wakeupper)
    : BasicHttpSession(eventLoop, socket, yield, peer_address, wakeupper)
{}

void SocketSession::shutdown() noexcept
{
    ::shutdown(m_socket, SHUT_RDWR);
}

ssize_t SocketSession::readSome(MutableBuffer buff, std::error_code &ec) noexcept
{
    ssize_t res = ::read(m_socket, buff.ptr, buff.length);
    if (res < 0) {
        if (errno != EAGAIN) {
            ec = std::make_error_code(std::errc(errno));
        } else {
            ec = {};
            res = 0;
        }
    } else {
        ec = {};
    }
    return res;
}

ssize_t SocketSession::writeSome(Dracon::ConstBuffer buff, std::error_code &ec) noexcept
{
    ssize_t res = ::write(m_socket, buff.ptr, buff.length);
    if (res < 0) {
        if (errno != EAGAIN) {
            ec = std::make_error_code(std::errc(errno));
        } else {
            ec = {};
            res = 0;
        }
    } else {
        ec = {};
        m_can_write_errror = false;
    }
    return res;
}

ssize_t SocketSession::writeSome(std::vector<Dracon::ConstBuffer> buff, std::error_code &ec) noexcept
{
    ssize_t res = ::writev(m_socket, reinterpret_cast<iovec*>(&buff[0]), buff.size());
    if (res < 0) {
        if (errno != EAGAIN) {
            ec = std::make_error_code(std::errc(errno));
        } else {
            ec = {};
            res = 0;
        }
    } else {
        ec = {};
        m_can_write_errror = false;
    }
    return res;
}

SslSocketSession::SslSocketSession(SessionsEventLoop *eventLoop, int socket, YieldType &yield, const sockaddr_storage &peer_address, const std::shared_ptr<AbstractWakeupper> &wakeupper)
    : BasicHttpSession(eventLoop, socket, yield, peer_address, wakeupper)
    , m_SSL(std::unique_ptr<SSL, void (*)(SSL *)>(SSL_new(Server::instance()->sslContext()), SSL_free))
{
    if (!m_SSL)
        throw std::runtime_error(ERR_error_string(ERR_get_error(), nullptr));

    if (!SSL_set_fd(m_SSL.get(), m_socket))
        throw std::runtime_error(ERR_error_string(SSL_get_error(m_SSL.get(), 0), nullptr));

    sessionTimeout(5s);
    int ret;
    while ((ret = SSL_accept(m_SSL.get())) != 1) {
        int err = SSL_get_error(m_SSL.get(), ret);
        switch (err) {
        case SSL_ERROR_WANT_READ:
        case SSL_ERROR_WANT_WRITE:
            if (auto ec = m_yield().get())
                throw ec;
            continue;
        default:
            throw std::make_error_code(std::errc::io_error);
        }
    }
    if (SSL_is_init_finished(m_SSL.get()) != 1)
        throw std::make_error_code(std::errc::connection_aborted);
}

void SslSocketSession::shutdown() noexcept
{
    sessionTimeout(2s);
    if (SSL_is_init_finished(m_SSL.get()) == 1) {
        int count = 5;
        while (count--) {
            int res = SSL_shutdown(m_SSL.get());
            if (!res && !m_yield().get())
                continue;
            break;
        }
    }
    ::shutdown(m_socket, SHUT_RDWR);
}

ssize_t SslSocketSession::readSome(MutableBuffer buff, std::error_code &ec) noexcept
{
    // make sure we start reading with no pending errors
    ERR_clear_error();
    auto sz = SSL_read(m_SSL.get(), buff.ptr, buff.length < INT_MAX ? int(buff.length) : INT_MAX);
    if (sz <= 0) {
        int err = SSL_get_error(m_SSL.get(), sz);
        switch (err) {
        case SSL_ERROR_WANT_READ:
        case SSL_ERROR_WANT_WRITE:
            ec = {};
            return 0;
        default:
            ec = std::make_error_code(std::errc::io_error);
            return -1;
        }
    }
    return sz;
}

ssize_t SslSocketSession::writeSome(Dracon::ConstBuffer buff, std::error_code &ec) noexcept
{
    // make sure we start writing with no pending errors
    ERR_clear_error();
    auto sz = SSL_write(m_SSL.get(), buff.ptr, buff.length < INT_MAX ? static_cast<int>(buff.length) : INT_MAX);
    if (sz <= 0) {
        int err = SSL_get_error(m_SSL.get(), sz);
        switch (err) {
        case SSL_ERROR_WANT_READ:
        case SSL_ERROR_WANT_WRITE:
            ec = {};
            return 0;
        default:
            ec = std::make_error_code(std::errc::io_error);
            return -1;
        }
    }
    m_can_write_errror = false;
    return sz;
}

ssize_t SslSocketSession::writeSome(std::vector<Dracon::ConstBuffer> buffers, std::error_code &ec) noexcept
{
    // Don't copy the buffers if the first piece is bigger than the socket write buffer size,
    // or we have only one buffer
    size_t socket_size = socketWriteSize();
    if ((buffers.size() && buffers[0].length >= socket_size) || buffers.size() == 1)
        return writeSome(buffers[0], ec);

    auto flat_buffer = m_eventLoop->shared_write_buffer(socket_size);
    flat_buffer->reset();
    char *pos = flat_buffer->data();
    const char *end = pos + std::min(flat_buffer->size(), socket_size);
    for (size_t i = 0; i < buffers.size() && pos != end; i++) {
        const size_t size = std::min<size_t>(buffers[i].length, end - pos);
        memcpy(pos, buffers[i].ptr, size);
        pos += size;
    }
    flat_buffer->setCurrentSize(pos - flat_buffer->data());
    return writeSome(flat_buffer->currentString(), ec);
}

} // namespace Getodac
