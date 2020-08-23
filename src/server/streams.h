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

#pragma once

#include <boost/coroutine2/coroutine.hpp>

#include <getodac/stream.h>
#include <getodac/utils.h>
#include <http_parser.h>
#include <openssl/err.h>
#include <openssl/ssl.h>

namespace Getodac {

using YieldType = boost::coroutines2::coroutine<std::error_code>::pull_type;
using Clock = std::chrono::high_resolution_clock;
using TimePoint = std::chrono::time_point<Clock>;
using namespace std::chrono_literals;

class sessions_event_loop;

struct mutable_buffer
{
    mutable_buffer() = default;
    mutable_buffer(std::string &data)
        : ptr(&data[0])
        , length(data.length())
    {}
    mutable_buffer(void *ptr, size_t length)
        : ptr(ptr)
        , length(length)
    {}
    void *ptr = nullptr;
    size_t length = 0;
};

class basic_http_session : public abstract_stream
{
public:
    basic_http_session(sessions_event_loop *eventLoop, int socket, YieldType &yield, const sockaddr_storage& peer_address, const std::shared_ptr<abstract_wakeupper> &wakeupper);
    ~basic_http_session() override;

    // abstract_stream interface
    void read(request &req) noexcept(false) override;
    void write(const_buffer buffer) noexcept(false) override;
    void write(std::vector<const_buffer> buffers) noexcept(false) override;

    std::error_code yield() noexcept override;
    std::shared_ptr<abstract_wakeupper> wakeupper() const noexcept override;

    void keep_alive(std::chrono::seconds seconds) noexcept override;
    std::chrono::seconds keep_alive() const noexcept override;

    const sockaddr_storage& peer_address() const noexcept override;

    int socket_write_size() const noexcept(false) override;
    void socket_write_size(int size) noexcept(false) override;

    int socket_read_size() const noexcept(false) override;
    void socket_read_size(int size) noexcept(false) override;

    std::chrono::seconds session_timeout() const noexcept override;
    void session_timeout(std::chrono::seconds seconds) noexcept override;

    TimePoint next_timeout() const noexcept;

    void io_loop();

protected:
    static int messageBegin(http_parser *parser);
    static int url(http_parser *parser, const char *at, size_t length);
    static int headerField(http_parser *parser, const char *at, size_t length);
    static int headerValue(http_parser *parser, const char *at, size_t length);
    static int headersComplete(http_parser *parser);
    static int body(http_parser *parser, const char *at, size_t length);
    static int messageComplete(http_parser *parser);
    virtual void shutdown() noexcept = 0;
    virtual ssize_t read_some(mutable_buffer buff, std::error_code &ec) noexcept = 0;
    virtual ssize_t write_some(const_buffer buff, std::error_code &ec) noexcept = 0;
    virtual ssize_t write_some(std::vector<const_buffer> buff, std::error_code &ec) noexcept = 0;

    request read_headers();

protected:
    YieldType &m_yield;
    int m_socket;
    sockaddr_storage m_peer_address;
    std::chrono::seconds m_keep_alive{0};
    std::chrono::seconds m_session_timeout{0};
    TimePoint m_session_timeout_time_point;
    sessions_event_loop *m_eventLoop;

    http_parser m_parser;
    http_parser_settings m_settings;
    bool m_can_write_errror = false;
    std::shared_ptr<abstract_wakeupper> m_wakeupper;
    char_buffer m_http_parser_buffer;
};

class socket_session : public basic_http_session
{
public:
    socket_session(Getodac::sessions_event_loop *eventLoop, int socket, YieldType &yield, const sockaddr_storage& peer_address, const std::shared_ptr<abstract_wakeupper> &wakeupper);

protected:
    // basic_http_session interface
    void shutdown() noexcept override;
    ssize_t read_some(mutable_buffer buff, std::error_code &ec) noexcept override;
    ssize_t write_some(const_buffer buff, std::error_code &ec) noexcept override;
    ssize_t write_some(std::vector<const_buffer> buff, std::error_code &ec) noexcept override;
};

class ssl_socket_session: public basic_http_session
{
public:
    ssl_socket_session(Getodac::sessions_event_loop *eventLoop, int socket, YieldType &yield, const sockaddr_storage& peer_address, const std::shared_ptr<abstract_wakeupper> &wakeupper);

protected:
    // basic_http_session interface
    void shutdown() noexcept override;
    ssize_t read_some(mutable_buffer buff, std::error_code &ec) noexcept override;
    ssize_t write_some(const_buffer buff, std::error_code &ec) noexcept override;
    ssize_t write_some(std::vector<const_buffer> buffers, std::error_code &ec) noexcept override;
    bool is_secured_connection() const noexcept final { return true; }

private:
    std::unique_ptr<SSL, void (*)(SSL *)> m_SSL;
};
} // namespace Getodac
