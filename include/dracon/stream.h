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

#include <chrono>
#include <memory>
#include <sstream>
#include <vector>
#include <system_error>

#include <sys/socket.h>

#include <dracon/utils.h>

namespace Dracon {

namespace {
    static const std::string_view EndOfChunckedStream{"0\r\n\r\n"};
    static const std::string_view CrlfString = {"\r\n"};
}

struct ConstBuffer
{
    ConstBuffer() = default;
    ConstBuffer(const void *ptr, size_t length)
        : ptr(ptr)
        , length(length)
    {}
    ConstBuffer(std::string_view data)
        : ptr(data.data())
        , length(data.size())
    {}
    ConstBuffer(const std::string &data)
        : ptr(data.data())
        , length(data.size())
    {}
    union {
        const void *ptr = nullptr;
        const char *c_ptr;
    };
    size_t length = 0;
};

class Request;
class AbstractStream
{
public:
    struct AbstractWakeupper
    {
        virtual ~AbstractWakeupper() = default;
        virtual void wakeUp() noexcept = 0;
    };

public:
    virtual ~AbstractStream() = default;
    /*!
     * \brief read
     *  Reads the data from socket into the specified \a buffer.
     * Throws an exception on error.
     */
    virtual void read(Request &req) = 0;

    /*!
     * \brief write
     * Writes the specified \a buffer to socket.
     */
    virtual void write(ConstBuffer buffer) = 0;

    /*!
     * \brief write
     * Writes the specified \a buffers to socket.
     */
    // TODO use std::span
    virtual void write(std::vector<ConstBuffer> buffers) = 0;

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
    virtual std::shared_ptr<AbstractWakeupper> wakeupper() const noexcept = 0;
    /*!
     * \brief keep_alive
     * The number of \a seconds to keep the connection alive after
     * we'll send the response. Setting it to 0 will close the connection
     * immediately.
     * Usually this value is initialized to seconds by the
     *      operator>>(abstract_stream &stream, request &request)
     * if the it was requested.
     */
    virtual void setKeepAlive(std::chrono::seconds seconds) noexcept = 0;

    /*!
     * \brief keep_alive
     * \return the number of seconds to keep the connection alive.
     */
    virtual std::chrono::seconds keepAlive() const noexcept = 0;

    /*!
     * \brief peer_address
     * \return the peer address structure
     */
    virtual const sockaddr_storage& peerAddress() const noexcept = 0;

    /*!
     * \brief is_secured_connection
     * \return true if this is a SSL connection
     */
    virtual bool isSecuredConnection() const noexcept { return false; }

    /*!
     * \brief send_buffer_size
     * \return the socket send buffer size in bytes
     */
    virtual int socketWriteSize() const = 0;

    /*!
     * \brief send_buffer_size
     *
     * Sets the socket sending buffer size
     *
     * \param size in bytes
     */
    virtual void socketWriteSize(int size) = 0;

    /*!
     * \brief receive_buffer_size
     * \return the socket receive buffer size in bytes
     */
    virtual int socketReadSize() const = 0;

    /*!
     * \brief receive_buffer_size
     *
     * Sets the socket receiving buffer size
     *
     * \param size in bytes
     */
    virtual void socketReadSize(int size) = 0;

    /*!
     * \brief expires_after
     * \return the entire session timeout.
     */
    virtual std::chrono::seconds sessionTimeout() const noexcept = 0;

    /*!
     * \brief expires_after
     * Sets the timeout for the entire session, starting from now.
     */
    virtual void setSessionTimeout(std::chrono::seconds seconds) noexcept = 0;
};

class NextLayerStream : public AbstractStream
{
public:
    NextLayerStream(AbstractStream &nextLayer)
        : m_nextLayer(nextLayer)
    {}

    // abstract_stream interface
    void read(Request &req) override
    {
        m_nextLayer.read(req);
    }

    std::error_code yield() noexcept override
    {
        return m_nextLayer.yield();
    }

    std::shared_ptr<AbstractWakeupper> wakeupper() const noexcept override
    {
        return m_nextLayer.wakeupper();
    }

    void setKeepAlive(std::chrono::seconds seconds) noexcept override
    {
        m_nextLayer.setKeepAlive(seconds);
    }

    std::chrono::seconds keepAlive() const noexcept override
    {
        return m_nextLayer.keepAlive();
    }

    const sockaddr_storage& peerAddress() const noexcept override
    {
        return m_nextLayer.peerAddress();
    }

    bool isSecuredConnection() const noexcept override
    {
        return m_nextLayer.isSecuredConnection();
    }

    int socketWriteSize() const override
    {
        return m_nextLayer.socketWriteSize();
    }

    void socketWriteSize(int size) override
    {
        m_nextLayer.socketWriteSize(size);
    }

    int socketReadSize() const override
    {
        return m_nextLayer.socketReadSize();
    }

    void socketReadSize(int size) override
    {
        m_nextLayer.socketReadSize(size);
    }

    std::chrono::seconds sessionTimeout() const noexcept override
    {
        return m_nextLayer.sessionTimeout();
    }

    void setSessionTimeout(std::chrono::seconds seconds) noexcept override
    {
        m_nextLayer.setSessionTimeout(seconds);
    }

protected:
    AbstractStream &m_nextLayer;
};


class ChunkedStream final : public NextLayerStream
{
public:
    ChunkedStream(AbstractStream &nextLayer)
        : NextLayerStream(nextLayer)
    {}

    ~ChunkedStream()
    {
        try {
            m_nextLayer.write(EndOfChunckedStream);
        } catch (...) {}
    }

    // abstract_stream interface
    void write(ConstBuffer buff) final
    {
        if (!buff.length)
            return;
        std::vector<ConstBuffer> buffers{3};
        std::ostringstream chunkHeaderBuf;
        chunkHeaderBuf << std::hex << buff.length << CrlfString;
        auto chunkHeader = chunkHeaderBuf.str();
        chunkHeaderBuf.str({});
        m_nextLayer.write({chunkHeader, buff, CrlfString});
    }

    void write(std::vector<ConstBuffer> buffers) final
    {
        std::vector<ConstBuffer> _buffers{1};
        size_t size = 0;
        for (auto buffer : buffers) {
            size += buffer.length;
            _buffers.push_back(buffer);
        }
        if (!size)
            return;
        _buffers.push_back(CrlfString);
        std::ostringstream chunkHeaderBuf;
        chunkHeaderBuf << std::hex << size << CrlfString;
        auto chunkHeader = chunkHeaderBuf.str();
        chunkHeaderBuf.str({});
        _buffers[0] = chunkHeader;
        m_nextLayer.write(_buffers);
    }
};

inline AbstractStream& operator<<(AbstractStream &stream, ConstBuffer buff)
{
    stream.write(buff);
    return stream;
}

class OStreamBuffer : public std::streambuf
{
public:
    OStreamBuffer(AbstractStream &stream)
        : m_stream(stream)
    {
        reserveBuffer();
    }

    ~OStreamBuffer() override
    {
        sync();
    }

protected:
    void reserveBuffer()
    {
        m_buffer.reserve(m_stream.socketWriteSize());
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
        }
        return __n;
    }


private:
    AbstractStream &m_stream;
    std::string m_buffer;
};

} // namespace dracon
