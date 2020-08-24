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

#include <chrono>
#include <memory>
#include <sstream>
#include <vector>
#include <system_error>

#include <sys/socket.h>

#include <getodac/utils.h>

namespace Getodac {

namespace {
    static const std::string_view end_of_chuncked_stream{"0\r\n\r\n"};
    static const std::string_view crlf_string = {"\r\n"};
}

struct const_buffer
{
    const_buffer() = default;
    const_buffer(const void *ptr, size_t length)
        : ptr(ptr)
        , length(length)
    {}
    const_buffer(std::string_view data)
        : ptr(data.data())
        , length(data.size())
    {}
    const_buffer(const std::string &data)
        : ptr(data.data())
        , length(data.size())
    {}
    union {
        const void *ptr = nullptr;
        const char *c_ptr;
    };
    size_t length = 0;
};

class request;
class abstract_stream
{
public:
    struct abstract_wakeupper
    {
        virtual ~abstract_wakeupper() = default;
        virtual void wake_up() noexcept = 0;
    };

public:
    virtual ~abstract_stream() = default;
    /*!
     * \brief read
     *  Reads the data from socket into the specified \a buffer.
     * Throws an exception on error.
     */
    virtual void read(request &req) = 0;

    /*!
     * \brief write
     * Writes the specified \a buffer to socket.
     */
    virtual void write(const_buffer buffer) = 0;

    /*!
     * \brief write
     * Writes the specified \a buffers to socket.
     */
    // TODO use std::span
    virtual void write(std::vector<const_buffer> buffers) = 0;

    /*!
     * \brief yield
     * Yields (pauses) the excetion of the current function.
     * The execution can be resume later by calling \l wakeup().
     */
    virtual std::error_code yield() noexcept = 0;

    /*!
     * \brief wakeupper
     * This object is used to wakeup an yield session
     */
    virtual std::shared_ptr<abstract_wakeupper> wakeupper() const noexcept = 0;
    /*!
     * \brief keep_alive
     * The number of \a seconds to keep the connection alive after
     * we'll send the response. Setting it to 0 will close the connection
     * immediately.
     * Usually this value is initialized to seconds by the
     *      operator>>(abstract_stream &stream, request &request)
     * if the it was requested.
     */
    virtual void keep_alive(std::chrono::seconds seconds) noexcept = 0;

    /*!
     * \brief keep_alive
     * \return the number of seconds to keep the connection alive.
     */
    virtual std::chrono::seconds keep_alive() const noexcept = 0;

    /*!
     * \brief peer_address
     * \return the peer address structure
     */
    virtual const sockaddr_storage& peer_address() const noexcept = 0;

    /*!
     * \brief is_secured_connection
     * \return true if this is a SSL connection
     */
    virtual bool is_secured_connection() const noexcept { return false; }

    /*!
     * \brief send_buffer_size
     * \return the socket send buffer size in bytes
     */
    virtual int socket_write_size() const = 0;

    /*!
     * \brief send_buffer_size
     *
     * Sets the socket sending buffer size
     *
     * \param size in bytes
     */
    virtual void socket_write_size(int size) = 0;

    /*!
     * \brief receive_buffer_size
     * \return the socket receive buffer size in bytes
     */
    virtual int socket_read_size() const = 0;

    /*!
     * \brief receive_buffer_size
     *
     * Sets the socket receiving buffer size
     *
     * \param size in bytes
     */
    virtual void socket_read_size(int size) = 0;

    /*!
     * \brief expires_after
     * \return the entire session timeout.
     */
    virtual std::chrono::seconds session_timeout() const noexcept = 0;

    /*!
     * \brief expires_after
     * Sets the timeout for the entire session, starting from now.
     */
    virtual void session_timeout(std::chrono::seconds seconds) noexcept = 0;
};

class next_layer_stream : public abstract_stream
{
public:
    next_layer_stream(abstract_stream &next_layer)
        : m_next_layer(next_layer)
    {}

    // abstract_stream interface
    void read(request &req) override
    {
        m_next_layer.read(req);
    }

    std::error_code yield() noexcept override
    {
        return m_next_layer.yield();
    }

    std::shared_ptr<abstract_wakeupper> wakeupper() const noexcept override
    {
        return m_next_layer.wakeupper();
    }

    void keep_alive(std::chrono::seconds seconds) noexcept override
    {
        m_next_layer.keep_alive(seconds);
    }

    std::chrono::seconds keep_alive() const noexcept override
    {
        return m_next_layer.keep_alive();
    }

    const sockaddr_storage& peer_address() const noexcept override
    {
        return m_next_layer.peer_address();
    }

    bool is_secured_connection() const noexcept override
    {
        return m_next_layer.is_secured_connection();
    }

    int socket_write_size() const override
    {
        return m_next_layer.socket_write_size();
    }

    void socket_write_size(int size) override
    {
        m_next_layer.socket_write_size(size);
    }

    int socket_read_size() const override
    {
        return m_next_layer.socket_read_size();
    }

    void socket_read_size(int size) override
    {
        m_next_layer.socket_read_size(size);
    }

    std::chrono::seconds session_timeout() const noexcept override
    {
        return m_next_layer.session_timeout();
    }

    void session_timeout(std::chrono::seconds seconds) noexcept override
    {
        m_next_layer.session_timeout(seconds);
    }

protected:
    abstract_stream &m_next_layer;
};


class chunked_stream : public next_layer_stream
{
public:
    chunked_stream(abstract_stream &next_layer)
        : next_layer_stream(next_layer)
    {}

    ~chunked_stream()
    {
        try {
            m_next_layer.write(end_of_chuncked_stream);
        } catch (...) {}
    }

    // abstract_stream interface
    void write(const_buffer buff) override
    {
        if (!buff.length)
            return;
        std::vector<const_buffer> buffers{3};
        std::ostringstream chunkHeaderBuf;
        chunkHeaderBuf << std::hex << buff.length << crlf_string;
        auto chunkHeader = chunkHeaderBuf.str();
        chunkHeaderBuf.str({});
        m_next_layer.write({chunkHeader, buff, crlf_string});
    }

    void write(std::vector<const_buffer> buffers) override
    {
        std::vector<const_buffer> _buffers{1};
        size_t size = 0;
        for (auto buffer : buffers) {
            size += buffer.length;
            _buffers.push_back(buffer);
        }
        if (!size)
            return;
        _buffers.push_back(crlf_string);
        std::ostringstream chunkHeaderBuf;
        chunkHeaderBuf << std::hex << size << crlf_string;
        auto chunkHeader = chunkHeaderBuf.str();
        chunkHeaderBuf.str({});
        _buffers[0] = chunkHeader;
        m_next_layer.write(_buffers);
    }
};

inline abstract_stream& operator<<(abstract_stream &stream, const_buffer buff)
{
    stream.write(buff);
    return stream;
}

class ostreambuffer : public std::streambuf
{
public:
    ostreambuffer(abstract_stream &stream)
        : m_stream(stream)
    {
        reserveBuffer();
    }

    ~ostreambuffer() override
    {
        sync();
    }

protected:
    void reserveBuffer()
    {
        m_buffer.reserve(m_stream.socket_write_size());
    }

    int sync() override
    {
        if (!m_buffer.empty()) {
            m_stream.write(m_buffer);
            m_buffer.clear();
            reserveBuffer();
        }
        return 0;
    }

    int_type overflow(int_type __c) override
    {
        if (m_buffer.size() >= m_buffer.capacity())
            sync();
        m_buffer += traits_type::to_char_type(__c);
        return 1;
    }

    std::streamsize xsputn(const char_type *__s, std::streamsize __n) override
    {
        if (__n < 0)
            return __n;
        auto sz = size_t(__n);
        while (sz) {
            if (sz > m_buffer.capacity()) {
                if (m_buffer.size()) {
                    m_stream.write({m_buffer, std::string_view{__s, sz}});
                } else {
                    m_stream.write(std::string_view{__s, sz});
                }
                m_buffer.clear();
                reserveBuffer();
                return sz;
            }
            if (m_buffer.size() >= m_buffer.capacity())
                sync();
            size_t len = std::min(size_t(sz), m_buffer.capacity() - m_buffer.size());
            m_buffer.append(__s, len);
            sz -= len;
            __s += len;
        };
        return __n;
    }


private:
    abstract_stream &m_stream;
    std::string m_buffer;
};

} // namespace Getodac
