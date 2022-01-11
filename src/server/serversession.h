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

#include "sessionseventloop.h"
#include "serverlogger.h"

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

namespace Getodac {

using Clock = std::chrono::high_resolution_clock;
using TimePoint = std::chrono::time_point<Clock>;

struct Wakeupper : Dracon::AbstractStream::AbstractWakeupper
{
    Wakeupper(int fd, uint64_t ptr)
        : m_fd(fd)
        , m_ptr(ptr)
    {}
    // abstract_wakeupper interface
    void wakeup() noexcept override
    {
        eventfd_write(m_fd, m_ptr);
    }
    int m_fd;
    uint64_t m_ptr;
};

class BasicServerSession
{
public:
    BasicServerSession(SessionsEventLoop *event_loop, int sock, std::string sock_addr, uint32_t order);
    virtual ~BasicServerSession();
    void initSession();

    inline uint32_t order() const noexcept { return m_order; }
    inline int sock() const noexcept { return m_sock;}
    const std::string &peerAddress() const noexcept;

    void setNextTimeout(std::chrono::seconds seconds) noexcept
    {
        m_nextTimeout = Clock::now() + seconds;
    }
    // basic_server_session interface
    TimePoint nextTimeout() const noexcept
    {
        std::unique_lock<std::mutex> lock{m_streamMutex};
        if (m_stream)
            return m_stream->nextTimeout();
        return m_nextTimeout;
    }

    virtual void processEvents(uint32_t events) noexcept = 0;
    virtual void timeout() noexcept = 0;
    virtual void wakeup() noexcept = 0;

protected:
    int m_sock;
    uint32_t m_order;
    std::string m_peerAddr;
    SessionsEventLoop *m_eventLoop;
    mutable std::mutex m_streamMutex;
    TimePoint m_nextTimeout;
    std::unique_ptr<BasicHttpSession> m_stream;
};

template <typename SocketStream>
class ServerSession : public BasicServerSession
{
    static_assert(std::is_base_of<BasicHttpSession, SocketStream>::value, "SocketStream must subclass basic_http_session");
public:
    ServerSession(SessionsEventLoop *eventLoop, int sock, std::string &&sockAddr, uint32_t order)
        : BasicServerSession(eventLoop, sock, std::move(sockAddr), order)
        , m_ioYield(std::bind(&ServerSession::ioLoop, this, std::placeholders::_1))
    {
        TRACE(Getodac::ServerLogger) << (void*)this
                                     << " eventLoop: " << eventLoop
                                     << " socket:" << sock;
        int opt = 1;
        if (setsockopt(m_sock, SOL_TCP, TCP_NODELAY, &opt, sizeof(int)))
            throw std::runtime_error{"Can't set socket option TCP_NODELAY"};
        setNextTimeout(Server::headersTimeout());
    }

    ~ServerSession() override
    {
        quitIoLoop(std::make_error_code(std::errc::operation_canceled));
        try {
            m_eventLoop->unregisterSession(this);
        } catch (...) {}
        ::close(m_sock);
    }

    void quitIoLoop(std::error_code ec)
    {
        try {
            while (m_ioYield) m_ioYield(ec);
        } catch (const std::error_code &ec) {
            ERROR(ServerLogger) << ec.message();
        } catch (const std::exception &e) {
            ERROR(ServerLogger) << e.what();
        } catch (...) {
            ERROR(ServerLogger) << "Unhandled error";
        }
    }

    void processEvents(uint32_t events) noexcept override
    {
        try {
            if (events & (EPOLLERR | EPOLLRDHUP | EPOLLHUP)) {
                quitIoLoop(std::make_error_code(std::errc::io_error));
                m_eventLoop->deleteLater(this);
            } else if (events & (EPOLLIN | EPOLLPRI | EPOLLOUT)) {
                if (m_ioYield) {
                    m_ioYield({});
                } else {
                    m_eventLoop->deleteLater(this);
                }
            } else {
                WARNING(ServerLogger) << "Unhandled epool events " << events;
                m_eventLoop->deleteLater(this);
            }
        } catch (const std::exception &e) {
            DEBUG(ServerLogger) << m_peerAddr << e.what();
            m_eventLoop->deleteLater(this);
        } catch (...) {
            DEBUG(ServerLogger) << m_peerAddr << "Unkown exception, terminating the session";
            m_eventLoop->deleteLater(this);
        }
    }

    void timeout() noexcept override
    {
        quitIoLoop(std::make_error_code(std::errc::timed_out));
        m_eventLoop->deleteLater(this);
    }

    void wakeup() noexcept override
    {
        try {
            if (m_ioYield)
                m_ioYield({});
            else
                m_eventLoop->deleteLater(this);
        } catch (const std::exception &e) {
            ERROR(ServerLogger) << e.what();
            m_eventLoop->deleteLater(this);
        } catch (...) {
            ERROR(ServerLogger) << "Unhandled error";
            m_eventLoop->deleteLater(this);
        }
    }

protected:
    void ioLoop(YieldType &yield)
    {
        try {
            {
                auto wu = std::make_shared<Wakeupper>(m_eventLoop->eventFd(),
                                                      uint64_t(static_cast<BasicServerSession*>(this)));
                auto stream = std::make_unique<SocketStream>(m_eventLoop, m_sock,
                                                             yield, m_peerAddr,
                                                             wu);
                std::unique_lock<std::mutex> lock{m_streamMutex};
                m_stream = std::move(stream);
            }
            m_stream->ioLoop();
        } catch(...) {
            m_eventLoop->deleteLater(this);
        }
    }

protected:
    using Call = boost::coroutines2::coroutine<std::error_code>::push_type;
    Call m_ioYield;
};

} // namespace Getodac
