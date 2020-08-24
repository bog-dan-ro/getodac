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

#include "sessions_event_loop.h"
#include "server_logger.h"

#include "http_parser.h"

#include <netinet/tcp.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/socket.h>
#include <unistd.h>

#include <chrono>
#include <memory>
#include <mutex>
#include <type_traits>

#include <boost/coroutine2/coroutine.hpp>

#include "server.h"
#include "streams.h"

using namespace std::chrono_literals;

namespace Getodac {

using Clock = std::chrono::high_resolution_clock;
using TimePoint = std::chrono::time_point<Clock>;

struct wakeupper : abstract_stream::abstract_wakeupper
{
    wakeupper(int fd, uint64_t ptr)
        : m_fd(fd)
        , m_ptr(ptr)
    {}
    // abstract_wakeupper interface
    void wake_up() noexcept override
    {
        eventfd_write(m_fd, m_ptr);
    }
    int m_fd;
    uint64_t m_ptr;
};

class basic_server_session
{
public:
    basic_server_session(sessions_event_loop *event_loop, int sock, const sockaddr_storage &sock_addr, uint32_t order);
    virtual ~basic_server_session();
    void init_session();

    inline uint32_t order() const noexcept { return m_order; }
    inline int sock() const noexcept { return m_sock;}
    const sockaddr_storage &peer_address() const noexcept;

    void next_timeout(std::chrono::seconds seconds) noexcept
    {
        m_next_timeout = Clock::now() + seconds;
    }
    // basic_server_session interface
    TimePoint next_timeout() const noexcept
    {
        std::unique_lock<std::mutex> lock{m_stream_mutex};
        if (m_stream)
            return m_stream->next_timeout();
        return m_next_timeout;
    }

    virtual void process_events(uint32_t events) noexcept = 0;
    virtual void timeout() noexcept = 0;
    virtual void wake_up() noexcept = 0;

protected:
    int m_sock;
    int m_order;
    struct sockaddr_storage m_peer_addr;
    sessions_event_loop *m_event_loop;
    mutable std::mutex m_stream_mutex;
    std::unique_ptr<basic_http_session> m_stream;
    TimePoint m_next_timeout;
};

template <typename SocketStream>
class server_session : public basic_server_session
{
    static_assert(std::is_base_of<basic_http_session, SocketStream>::value, "SocketStream must subclass basic_http_session");
public:
    server_session(sessions_event_loop *eventLoop, int sock, const sockaddr_storage &sockAddr, uint32_t order)
        : basic_server_session(eventLoop, sock, sockAddr, order)
        , m_io_yield(std::bind(&server_session::io_loop, this, std::placeholders::_1))
    {
        TRACE(Getodac::server_logger) << (void*)this
                                     << " eventLoop: " << eventLoop
                                     << " socket:" << sock;
        int opt = 1;
        if (setsockopt(m_sock, SOL_TCP, TCP_NODELAY, &opt, sizeof(int)))
            throw std::runtime_error{"Can't set socket option TCP_NODELAY"};
        next_timeout(5s);
    }

    ~server_session() override
    {
        quit_io_loop(std::make_error_code(std::errc::operation_canceled));
        try {
            m_event_loop->unregister_session(this);
        } catch (...) {}
        ::close(m_sock);
    }

    void quit_io_loop(std::error_code ec)
    {
        try {
            while (m_io_yield) m_io_yield(ec);
        } catch (const std::error_code &ec) {
            ERROR(server_logger) << ec.message();
        } catch (const std::exception &e) {
            ERROR(server_logger) << e.what();
        } catch (...) {
            ERROR(server_logger) << "Unhandled error";
        }
    }

    void process_events(uint32_t events) noexcept override
    {
        try {
            if (events & (EPOLLERR | EPOLLRDHUP | EPOLLHUP)) {
                quit_io_loop(std::make_error_code(std::errc::io_error));
                m_event_loop->delete_later(this);
            } else if (events & (EPOLLIN | EPOLLPRI | EPOLLOUT)) {
                if (m_io_yield) {
                    m_io_yield({});
                } else {
                    m_event_loop->delete_later(this);
                }
            } else {
                WARNING(server_logger) << "Unhandled epool events " << events;
                m_event_loop->delete_later(this);
            }
        } catch (const std::exception &e) {
            DEBUG(server_logger) << addr_text(m_peer_addr) << e.what();
            m_event_loop->delete_later(this);
        } catch (...) {
            DEBUG(server_logger) << addr_text(m_peer_addr) << "Unkown exception, terminating the session";
            m_event_loop->delete_later(this);
        }
    }

    void timeout() noexcept override
    {
        quit_io_loop(std::make_error_code(std::errc::timed_out));
        m_event_loop->delete_later(this);
    }

    void wake_up() noexcept override
    {
        try {
            if (m_io_yield)
                m_io_yield({});
            else
                m_event_loop->delete_later(this);
        } catch (const std::exception &e) {
            ERROR(server_logger) << e.what();
            m_event_loop->delete_later(this);
        } catch (...) {
            ERROR(server_logger) << "Unhandled error";
            m_event_loop->delete_later(this);
        }
    }

protected:
    void io_loop(YieldType &yield)
    {
        try {
            {
                auto wu = std::make_shared<wakeupper>(m_event_loop->event_fd(),
                                                      uint64_t(static_cast<basic_server_session*>(this)));
                auto stream = std::make_unique<SocketStream>(m_event_loop, m_sock,
                                                             yield, m_peer_addr,
                                                             wu);
                std::unique_lock<std::mutex> lock{m_stream_mutex};
                m_stream = std::move(stream);
            }
            m_stream->io_loop();
        } catch(...) {
            m_event_loop->delete_later(this);
        }
    }

protected:
    using Call = boost::coroutines2::coroutine<std::error_code>::push_type;
    Call m_io_yield;
};

} // namespace Getodac
