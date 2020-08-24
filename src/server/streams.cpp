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

    AGPL EXCEPTION:
    The AGPL license applies only to this file itself.

    As a special exception, the copyright holders of this file give you permission
    to use it, regardless of the license terms of your work, and to copy and distribute
    them under terms of your choice.
    If you do any changes to this file, these changes must be published under AGPL.
*/

#include "streams.h"

#include <sys/uio.h>

#include <getodac/http.h>
#include <getodac/logging.h>

#include "server.h"
#include "server_logger.h"
#include "sessions_event_loop.h"


namespace Getodac {

template<typename T>
using deleted_unique_ptr = std::unique_ptr<T,std::function<void(T*)>>;

using namespace std::chrono_literals;

struct http_parser_data
{
    request &req;
    std::string header_field;
};


basic_http_session::basic_http_session(sessions_event_loop *eventLoop, int socket, YieldType &yield, const sockaddr_storage &peer_address, const std::shared_ptr<abstract_wakeupper> &wakeupper)
    : m_yield(yield)
    , m_socket(socket)
    , m_peer_address(peer_address)
    , m_eventLoop(eventLoop)
    , m_wakeupper(wakeupper)
{
    memset(&m_settings, 0, sizeof(m_settings));
    m_settings.on_message_begin = &basic_http_session::messageBegin;
    m_settings.on_url = &basic_http_session::url;
    m_settings.on_header_field = &basic_http_session::headerField;
    m_settings.on_header_value = &basic_http_session::headerValue;
    m_settings.on_headers_complete = &basic_http_session::headersComplete;
    m_settings.on_body = &basic_http_session::body;
    m_settings.on_message_complete = &basic_http_session::messageComplete;
    http_parser_init(&m_parser, HTTP_REQUEST);
    session_timeout(5s);
}

basic_http_session::~basic_http_session() = default;

void basic_http_session::read(request &req) noexcept(false)
{
    if (req.state() == request::state::completed)
        return;

    m_can_write_errror = true;
    auto &buffer = m_eventLoop->shared_read_buffer;
    http_parser_data data{.req = req};
    m_parser.data = &data;
    if (m_http_parser_buffer.current_size()) {
        auto parsed_bytes = http_parser_execute(&m_parser, &m_settings, m_http_parser_buffer.current_data(), m_http_parser_buffer.current_size());
        if (m_parser.http_errno) {
            WARNING(Getodac::server_logger) << http_errno_name(http_errno(m_parser.http_errno));
            throw std::make_error_code(std::errc::bad_message);
        }
        m_http_parser_buffer.advance(parsed_bytes);
    }
    while (req.state() != request::state::completed) {
        buffer.reset();
        std::error_code ec;
        auto temp_size = m_http_parser_buffer.current_size();
        if (temp_size)
            buffer *= m_http_parser_buffer.current_string();
        auto sz = read_some({buffer.current_data(), buffer.current_size()}, ec);
        if (ec)
            throw ec;
        if (!sz) {
            if ((ec = m_yield().get()))
                throw ec;
            continue;
        }
        buffer.reset();
        buffer.set_current_size(sz + temp_size);
        auto parsed_bytes = http_parser_execute(&m_parser, &m_settings,
                                                buffer.current_data(),
                                                buffer.current_size());
        if (m_parser.http_errno) {
            WARNING(Getodac::server_logger) << http_errno_name(http_errno(m_parser.http_errno));
            throw std::make_error_code(std::errc::bad_message);
        }
        if (req.state() == request::state::completed)
            break;
        buffer.advance(parsed_bytes);
        if (buffer.current_size())
            m_http_parser_buffer = buffer.current_string();
        else
            m_http_parser_buffer.clear();
    }
    m_http_parser_buffer.clear();
}

void basic_http_session::write(const_buffer buffer) noexcept(false)
{
    while (buffer.length) {
        std::error_code ec;
        size_t written = write_some(buffer, ec);
        if (!written) {
            if ((ec = m_yield().get()))
                throw ec;
            continue;
        }
        if (ec)
            throw ec;
        *reinterpret_cast<const char**>(&buffer.ptr) += written;
        buffer.length -= written;
    }
}

void basic_http_session::write(std::vector<const_buffer> buffers) noexcept(false)
{
    for(;;) {
        std::error_code ec;
        auto written = write_some(buffers, ec);
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
                auto tmp = std::vector(buffers.begin() + i, buffers.end());
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

std::error_code Getodac::basic_http_session::yield() noexcept
{
    return m_yield().get();
}

std::shared_ptr<abstract_stream::abstract_wakeupper> basic_http_session::wakeupper() const noexcept
{
    return m_wakeupper;
}

void basic_http_session::keep_alive(std::chrono::seconds seconds) noexcept
{
    m_keep_alive = seconds;
}

std::chrono::seconds basic_http_session::keep_alive() const noexcept
{
    return m_keep_alive;
}

const sockaddr_storage &basic_http_session::peer_address() const noexcept
{
    return m_peer_address;
}

int basic_http_session::socket_write_size() const noexcept(false)
{
    int optval = 0;
    socklen_t optlen = sizeof(optval);
    getsockopt(m_socket, SOL_SOCKET, SO_SNDBUF, &optval, &optlen);
    return optval / 2;
}

void basic_http_session::socket_write_size(int size) noexcept(false)
{
    if (setsockopt(m_socket, SOL_SOCKET, SO_SNDBUF, &size, sizeof(int)))
        throw std::make_error_code(std::errc::invalid_argument);
}

int basic_http_session::socket_read_size() const noexcept(false)
{
    int optval = 0;
    socklen_t optlen = sizeof(optval);
    getsockopt(m_socket, SOL_SOCKET, SO_RCVBUF, &optval, &optlen);
    return optval / 2;
}

void basic_http_session::socket_read_size(int size) noexcept(false)
{
    if (setsockopt(m_socket, SOL_SOCKET, SO_RCVBUF, &size, sizeof(int)))
        throw std::make_error_code(std::errc::invalid_argument);
}

std::chrono::seconds basic_http_session::session_timeout() const noexcept
{
    return m_session_timeout;
}

void basic_http_session::session_timeout(std::chrono::seconds seconds) noexcept
{
    m_session_timeout = seconds;
    if (seconds.count())
        m_session_timeout_time_point = std::chrono::high_resolution_clock::now() + seconds;
    else
        m_session_timeout_time_point = {};
}

TimePoint basic_http_session::next_timeout() const noexcept
{
    return m_session_timeout_time_point;
}

void basic_http_session::io_loop()
{
    try {
        session_timeout(5s); // 5 seconds to read the headers
        do {
            request req = read_headers();
            keep_alive(req.keep_alive() * 10s);
            auto session = server::instance()->create_session(req);
            if (!session) {
                write(response{503}.to_string());
                break;
            }
            session(*this, req);
            session_timeout(keep_alive());
            server::instance()->session_served();
        } while (!m_yield.get() && m_keep_alive.count());
    } catch (int error) {
        DEBUG(Getodac::server_logger) << "http error " << error;
        if (m_can_write_errror) {
            write(response{error > 0 && error < std::numeric_limits<uint16_t>::max()
                           ? uint16_t(error)
                           : uint16_t(500)}.to_string());
        }
    } catch (const response &res) {
        DEBUG(Getodac::server_logger) << res.status_code() << " " << res.body();
        if (m_can_write_errror)
            write(res.to_string());
    } catch (const std::exception &e) {
        DEBUG(Getodac::server_logger) << e.what();
        if (m_can_write_errror)
            write(response{500, e.what()}.to_string());
    } catch (const std::error_code &ec) {
        DEBUG(Getodac::server_logger) << ec.message();
        if (m_can_write_errror)
            write(response{500, ec.message()}.to_string());
    } catch (...) {
        DEBUG(Getodac::server_logger) << "Unknown error";
        if (m_can_write_errror)
            write(response{}.to_string());
    }
    shutdown();
}

int basic_http_session::messageBegin(http_parser *parser)
{
    auto data = reinterpret_cast<http_parser_data*>(parser->data);
    data->req.state(request::state::processing_url);
    return 0;
}

int basic_http_session::url(http_parser *parser, const char *at, size_t length)
{
    auto data = reinterpret_cast<http_parser_data*>(parser->data);
    data->req.url(std::string{at, length});
    data->req.method(http_method_str(http_method(parser->method)));
    data->req.state(request::state::processing_header);
    return 0;
}

int basic_http_session::headerField(http_parser *parser, const char *at, size_t length)
{
    auto data = reinterpret_cast<http_parser_data*>(parser->data);
    data->header_field = std::string{at, length};
    return 0;
}

int basic_http_session::headerValue(http_parser *parser, const char *at, size_t length)
{
    auto data = reinterpret_cast<http_parser_data*>(parser->data);
    data->req[std::move(data->header_field)] = std::string{at, length};
    return 0;
}

int basic_http_session::headersComplete(http_parser *parser)
{
    auto data = reinterpret_cast<http_parser_data*>(parser->data);
    data->req.state(request::state::headers_completed);
    data->req.keep_alive(http_should_keep_alive(parser));
    return 0;
}

int basic_http_session::body(http_parser *parser, const char *at, size_t length)
{
    auto data = reinterpret_cast<http_parser_data*>(parser->data);
    data->req.append_body({at, length});
    return 0;
}

int basic_http_session::messageComplete(http_parser *parser)
{
    auto data = reinterpret_cast<http_parser_data*>(parser->data);
    data->req.state(request::state::completed);
    return 0;
}

request basic_http_session::read_headers()
{
    request req;
    http_parser_data data{.req = req};

    m_parser.data = &data;
    http_parser_init(&m_parser, HTTP_REQUEST);

    auto &buffer = m_eventLoop->shared_read_buffer;
    while(req.state() != request::state::headers_completed &&
        req.state() != request::state::completed) {
        buffer.reset();
        std::error_code ec;
        auto temp_size = m_http_parser_buffer.current_size();
        if (temp_size)
            buffer *= m_http_parser_buffer.current_data();
        auto sz = read_some({buffer.current_data(), buffer.current_size()}, ec);
        buffer.set_current_data(0);
        if (ec)
            throw ec;
        if (!sz) {
            if ((ec = m_yield().get()))
                throw ec;
            continue;
        }
        m_can_write_errror = true;
        buffer.reset();
        buffer.set_current_size(sz + temp_size);
        while ((req.state() != request::state::headers_completed &&
                req.state() != request::state::completed) &&
               buffer.current_size()) {
            // find next crlf
            auto next = buffer.current_string().find(crlf_string);
            if (next == std::string::npos)
                next = buffer.current_size();
            else
                next += 2;
            auto parsed_bytes = http_parser_execute(&m_parser, &m_settings, buffer.current_data(), next);
            if (m_parser.http_errno) {
                WARNING(Getodac::server_logger) << http_errno_name(http_errno(m_parser.http_errno));
                throw std::make_error_code(std::errc::bad_message);
            }
            buffer.advance(next);
            if (next != parsed_bytes)
                break;
        }
        if (buffer.current_size())
            m_http_parser_buffer = buffer.current_string();
        else
            m_http_parser_buffer.clear();
    }
    return req;
}


socket_session::socket_session(sessions_event_loop *eventLoop, int socket, YieldType &yield, const sockaddr_storage &peer_address, const std::shared_ptr<abstract_wakeupper> &wakeupper)
    : basic_http_session(eventLoop, socket, yield, peer_address, wakeupper)
{}

void socket_session::shutdown() noexcept
{
    ::shutdown(m_socket, SHUT_RDWR);
}

ssize_t socket_session::read_some(mutable_buffer buff, std::error_code &ec) noexcept
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

ssize_t socket_session::write_some(const_buffer buff, std::error_code &ec) noexcept
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

ssize_t socket_session::write_some(std::vector<const_buffer> buff, std::error_code &ec) noexcept
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

ssl_socket_session::ssl_socket_session(sessions_event_loop *eventLoop, int socket, YieldType &yield, const sockaddr_storage &peer_address, const std::shared_ptr<abstract_wakeupper> &wakeupper)
    : basic_http_session(eventLoop, socket, yield, peer_address, wakeupper)
    , m_SSL(std::unique_ptr<SSL, void (*)(SSL *)>(SSL_new(server::instance()->ssl_context()), SSL_free))
{
    if (!m_SSL)
        throw std::runtime_error(ERR_error_string(ERR_get_error(), nullptr));

    if (!SSL_set_fd(m_SSL.get(), m_socket))
        throw std::runtime_error(ERR_error_string(SSL_get_error(m_SSL.get(), 0), nullptr));

    session_timeout(5s);
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

void ssl_socket_session::shutdown() noexcept
{
    session_timeout(2s);
    int count = 5;
    if (SSL_is_init_finished(m_SSL.get()) == 1) {
        while (count--) {
            int res = SSL_shutdown(m_SSL.get());
            if (!res && !m_yield().get())
                continue;
            break;
        }
    }
    ::shutdown(m_socket, SHUT_RDWR);
}

ssize_t ssl_socket_session::read_some(mutable_buffer buff, std::error_code &ec) noexcept
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

ssize_t ssl_socket_session::write_some(const_buffer buff, std::error_code &ec) noexcept
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

ssize_t ssl_socket_session::write_some(std::vector<const_buffer> buffers, std::error_code &ec) noexcept
{
    // Don't copy the buffers if the first piece is bigger than the socket write buffer size,
    // or we have only one buffer
    size_t socket_size = socket_write_size();
    if ((buffers.size() && buffers[0].length >= socket_size) || buffers.size() == 1)
        return write_some(buffers[0], ec);

    auto flat_buffer = m_eventLoop->shared_write_buffer(socket_size);
    flat_buffer->reset();
    char *pos = flat_buffer->data();
    const char *end = pos + std::min(flat_buffer->size(), socket_size);
    for (size_t i = 0; i < buffers.size() && pos != end; i++) {
        const size_t size = std::min<size_t>(buffers[i].length, end - pos);
        memcpy(pos, buffers[i].ptr, size);
        pos += size;
    }
    flat_buffer->set_current_size(pos - flat_buffer->data());
    return write_some(flat_buffer->current_string(), ec);
}

} // namespace Getodac
