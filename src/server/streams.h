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

#pragma once

#include <boost/coroutine2/coroutine.hpp>

#include <dracon/stream.h>
#include <dracon/utils.h>
#include <http_parser.h>
#include <openssl/err.h>
#include <openssl/ssl.h>

namespace Getodac {

using YieldType = boost::coroutines2::coroutine<std::error_code>::pull_type;
using Clock = std::chrono::high_resolution_clock;
using TimePoint = std::chrono::time_point<Clock>;
using namespace std::chrono_literals;

class SessionsEventLoop;

struct MutableBuffer
{
    MutableBuffer() = default;
    MutableBuffer(std::string &data)
        : ptr(&data[0])
        , length(data.length())
    {}
    MutableBuffer(void *ptr, size_t length)
        : ptr(ptr)
        , length(length)
    {}
    void *ptr = nullptr;
    size_t length = 0;
};

class BasicHttpSession : public Dracon::AbstractStream
{
public:
    BasicHttpSession(SessionsEventLoop *eventLoop, int socket, YieldType &yield, const sockaddr_storage& peer_address, const std::shared_ptr<AbstractWakeupper> &wakeupper);
    ~BasicHttpSession() override;

    // abstract_stream interface
    void read(Dracon::Request &req) noexcept(false) override;
    void write(Dracon::ConstBuffer buffer) noexcept(false) override;
    void write(std::vector<Dracon::ConstBuffer> buffers) noexcept(false) override;

    std::error_code yield() noexcept override;
    std::shared_ptr<AbstractWakeupper> wakeupper() const noexcept override;

    void setKeepAlive(std::chrono::seconds seconds) noexcept override;
    std::chrono::seconds keepAlive() const noexcept override;

    const sockaddr_storage& peerAddress() const noexcept override;

    int socketWriteSize() const noexcept(false) override;
    void socketWriteSize(int size) noexcept(false) override;

    int socketReadSize() const noexcept(false) override;
    void socketReadSize(int size) noexcept(false) override;

    std::chrono::seconds sessionTimeout() const noexcept override;
    void setSessionTimeout(std::chrono::seconds seconds) noexcept override;

    TimePoint nextTimeout() const noexcept;

    void ioLoop();

protected:
    static int messageBegin(http_parser *parser);
    static int url(http_parser *parser, const char *at, size_t length);
    static int headerField(http_parser *parser, const char *at, size_t length);
    static int headerValue(http_parser *parser, const char *at, size_t length);
    static int headersComplete(http_parser *parser);
    static int body(http_parser *parser, const char *at, size_t length);
    static int messageComplete(http_parser *parser);
    virtual void shutdown() noexcept = 0;
    virtual ssize_t readSome(MutableBuffer buff, std::error_code &ec) noexcept = 0;
    virtual ssize_t writeSome(Dracon::ConstBuffer buff, std::error_code &ec) noexcept = 0;
    virtual ssize_t writeSome(std::vector<Dracon::ConstBuffer> buff, std::error_code &ec) noexcept = 0;

    Dracon::Request readHeaders();

protected:
    YieldType &m_yield;
    int m_socket;
    sockaddr_storage m_peerAddress;
    std::chrono::seconds m_keepAlive{0};
    std::chrono::seconds m_sessionTimeout{0};
    TimePoint m_sessionTimeoutTimePoint;
    SessionsEventLoop *m_eventLoop;

    http_parser m_parser;
    http_parser_settings m_settings;
    bool m_can_write_errror = false;
    std::shared_ptr<AbstractWakeupper> m_wakeupper;
    Dracon::CharBuffer m_httpParserBuffer;
};

class SocketSession final: public BasicHttpSession
{
public:
    SocketSession(Getodac::SessionsEventLoop *eventLoop, int socket, YieldType &yield, const sockaddr_storage& peer_address, const std::shared_ptr<AbstractWakeupper> &wakeupper);

protected:
    // basic_http_session interface
    void shutdown() noexcept final;
    ssize_t readSome(MutableBuffer buff, std::error_code &ec) noexcept final;
    ssize_t writeSome(Dracon::ConstBuffer buff, std::error_code &ec) noexcept final;
    ssize_t writeSome(std::vector<Dracon::ConstBuffer> buff, std::error_code &ec) noexcept final;
};

class SslSocketSession final: public BasicHttpSession
{
public:
    SslSocketSession(Getodac::SessionsEventLoop *eventLoop, int socket, YieldType &yield, const sockaddr_storage& peer_address, const std::shared_ptr<AbstractWakeupper> &wakeupper);

protected:
    // basic_http_session interface
    void shutdown() noexcept final;
    ssize_t readSome(MutableBuffer buff, std::error_code &ec) noexcept final;
    ssize_t writeSome(Dracon::ConstBuffer buff, std::error_code &ec) noexcept final;
    ssize_t writeSome(std::vector<Dracon::ConstBuffer> buffers, std::error_code &ec) noexcept final;
    bool isSecuredConnection() const noexcept final { return true; }

private:
    std::unique_ptr<SSL, void (*)(SSL *)> m_SSL;
};
} // namespace Getodac
