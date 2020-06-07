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

#include <memory>
#include <string>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

#include "abstract_server_session.h"
#include "exceptions.h"

namespace Getodac {
static const char crlf[] = "\r\n";

/*!
 * \brief The AbstractServiceSession class
 *
 * Throwing an exception from any of this class methods will terminate the session
 */
class AbstractServiceSession
{
public:
    explicit AbstractServiceSession(AbstractServerSession *serverSession)
        : m_serverSession(serverSession)
    {}

    virtual ~AbstractServiceSession() = default;

    /*!
     * \brief headerFieldValue
     *
     * AbstractServerSession calls this function when it has decoded a header filed value pair
     *
     * \param field the decoded field
     * \param value the decoded value
     */
    virtual void headerFieldValue(const std::string &field, const std::string &value) = 0;

    /*!
     * \brief acceptContentLength
     *
     * AbstractServerSession calls this function when it detects a "Expect: 100-continue" header
     *
     * \param length the "Content-Length"
     * \return true if the service accepts the body, in this case a "HTTP/1.1 100 Continue" will be sent immediately.
     *         False will send a "HTTP/1.1 417 Expectation Failed" and it will close the connection.
     */
    virtual bool acceptContentLength(size_t length) = 0;

    /*!
     * \brief headersComplete
     *
     *  AbstractServerSession calls this function when all the headers are parsed
     */
    virtual void headersComplete() = 0;

    /*!
     * \brief body
     *
     * AbstractServerSession calls this function (multiple times) when it decodes more body data
     *
     * \param data body data
     * \param length body length
     */
    virtual void body(const char *data, size_t length) = 0;

    /*!
     * \brief requestComplete
     *
     * AbstractServerSession calls this function when the request is completed.
     * From this moment the AbstractServiceSession must write the response headers
     */
    virtual void requestComplete() = 0;

    /*!
     * \brief writeResponse
     *
     * The AbstractServerSession calls this function after this service session
     * finished to write the headers data and when this service session can write data.
     * This service session can use yield to "pause" the execution anywhere it likes.
     * When a new write event occurs, it will resume the execution from that point.
     *
     * \param yield object usually used to pass it to AbstractServerSession::write & AbstractServerSession::writev methods
     */
    virtual void writeResponse(AbstractServerSession::Yield &yield) = 0;

    /*!
     * \brief writeChunkedData
     *
     * Helper function needed by a service session to write chunked encoded data
     *
     * \param yield object passed to writeResponse, this function will use it to wait until all the data is written.
     * \param data the buffer you want to write. If the data size is 0, it means end of chunked transfer.
     */
    void writeChunkedData(AbstractServerSession::Yield &yield, std::string_view data = {})
    {
        if (data.empty()) {
            m_serverSession->write(yield, "0\r\n\r\n");
            return;
        }
        iovec chunkData[3];
        std::ostringstream chunkHeaderBuf;
        chunkHeaderBuf << std::hex << data.size() << crlf;
        auto chunkHeader = chunkHeaderBuf.str();
        chunkHeaderBuf.str({});
        chunkData[0].iov_base = (void*)chunkHeader.c_str();
        chunkData[0].iov_len = chunkHeader.length();
        chunkData[1].iov_base = (void*)data.data();
        chunkData[1].iov_len = data.size();
        chunkData[2].iov_base = (void*)crlf;
        chunkData[2].iov_len = 2;
        m_serverSession->writev(yield, chunkData, 3);
    }

    inline size_t sendBufferSize() const { return m_serverSession->sendBufferSize(); }
    inline AbstractServerSession * serverSession() const { return m_serverSession; }
protected:
    AbstractServerSession *m_serverSession = nullptr;
};


class OStreamBuffer : public std::streambuf
{
    enum class Status {
        Invalid,
        Valid,
        Chuncked
    };

public:
    static inline int hexLen(size_t nr)
    {
        int sz = 0;
        do {
            nr >>= 4;
            ++sz;
        } while (nr);
        return sz;
    }

    OStreamBuffer(AbstractServiceSession *serverSession, AbstractServerSession::Yield &yield)
        : m_serviceSession(serverSession)
        , m_yield(yield)
    {}

    ~OStreamBuffer() override
    {
        try {
            sync();
            if (m_status == Status::Chuncked)
                m_serviceSession->writeChunkedData(m_yield);
        } catch (...) {}
    }

    inline AbstractServerSession::Yield &yield() const { return m_yield; }
    inline AbstractServiceSession *serviceSession() const { return m_serviceSession; };
    void writeHeaders(const ResponseHeaders &headers)
    {
        if (m_status != Status::Invalid)
            throw std::runtime_error{"ResponseHeaders already written"};
        m_status = headers.contentLength == Getodac::ChunkedData ? Status::Chuncked : Status::Valid;
        m_serviceSession->serverSession()->write(m_yield, headers);
        reserveBuffer();
    }

    // basic_streambuf interface
protected:
    void reserveBuffer()
    {
        if (m_status == Status::Chuncked)
            m_buffer.reserve(m_serviceSession->sendBufferSize() - hexLen(m_serviceSession->sendBufferSize()) - 4);
        else
            m_buffer.reserve(m_serviceSession->sendBufferSize());
    }

    int sync() override
    {
        if (m_status == Status::Invalid)
            throw std::runtime_error{"No ResponseHeaders where written"};
        if (!m_buffer.empty()) {
            if (m_status == Status::Chuncked)
                m_serviceSession->writeChunkedData(m_yield, m_buffer);
            else
                m_serviceSession->serverSession()->write(m_yield, m_buffer);
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
        std::streamsize sz = __n;
        while (sz) {
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
    Status m_status = Status::Invalid;
    AbstractServiceSession *m_serviceSession;
    AbstractServerSession::Yield &m_yield;
    std::string m_buffer;
};

class OStream : public std::ostream
{
public:
    explicit OStream(OStreamBuffer &buff)
        : std::ostream(&buff)
        , m_buff(buff) {}

    inline AbstractServerSession::Yield &yield() const { return m_buff.yield(); }
    inline OStreamBuffer &streamBuffer() const { return m_buff; }

private:
    OStreamBuffer &m_buff;
};

inline OStream & operator << (OStream &stream, const ResponseHeaders &headers)
{
    stream.streamBuffer().writeHeaders(headers);
    return stream;
}

template <typename BaseClass = Getodac::AbstractServiceSession>
class AbstractSimplifiedServiceSession : public BaseClass
{
public:
    struct RequestHeadersFilter
    {
        /*!
         * \brief acceptedHeades. Only this headers will be caputured, the rest of them will be ignored.
         */
        std::unordered_set<std::string> acceptedHeades;

        /*!
         * \brief strictHeaders. Closes the connection if it's true and a header
         * field is not found in acceptedHeades
         */
        bool strictHeaders = false;

        /*!
         * \brief maxHeaders. If the headers exceed this value the connection
         * wil be closed immediately.
         */
        size_t maxHeaders = 100;

        /*!
         * \brief maxValueLength, the maximum accepted header value length.
         *  The conection will be closed immediately if exceeded.
         */
        size_t maxValueLength = 4096;

        /*!
         * \brief maxKeyLength, the maximum accepted header value length.
         *  The conection will be closed immediately if exceeded.
         */
        size_t maxKeyLength = 512;
    };

public:
    explicit AbstractSimplifiedServiceSession(AbstractServerSession *serverSession)
        : AbstractServiceSession(serverSession)
    {}

    /*!
     * \brief writeResponse
     *  This method is called when you need to write the resounse. The session is ended when this function exits.
     *
     * \param stream. A stream object which can be used to write the response body.
     */
    virtual void writeResponse(OStream &stream) = 0;

    // AbstractServiceSession interface
protected:
    void headerFieldValue(const std::string &field, const std::string &value) override
    {
        if (!--m_requestHeadersFilter.maxHeaders ||
                field.size() > m_requestHeadersFilter.maxKeyLength ||
                value.size() > m_requestHeadersFilter.maxValueLength) {
            throw ResponseStatusError(431, "Invalid request headers");
        }

        if (m_requestHeadersFilter.acceptedHeades.find(field) != m_requestHeadersFilter.acceptedHeades.end())
            m_requestHeaders.emplace(field, value);
        else if (m_requestHeadersFilter.strictHeaders)
            throw ResponseStatusError(400, "Invalid request headers");

    }
    void headersComplete() override {}
    void requestComplete() override {}
    void writeResponse(Getodac::AbstractServerSession::Yield &yield) final
    {
        OStreamBuffer streamBuffer{this, yield};
        OStream stream(streamBuffer);
        writeResponse(stream);
    }

protected:
    HeadersData m_requestHeaders;
    RequestHeadersFilter m_requestHeadersFilter;
};

#define PLUGIN_EXPORT extern "C" __attribute__ ((visibility("default")))

/*!
 Every plugin must implement the following functions as PLUGIN_EXPORT functions:

{code}
PLUGIN_EXPORT bool initPlugin(const std::string &confDir)
{
  // This function is called by the server after it loads the plugin
}

PLUGIN_EXPORT uint32_t pluginOrder()
{
    // The server calls this function to get the plugin order
}

PLUGIN_EXPORT std::shared_ptr<AbstractServiceSession> createSession(AbstractServerSession *serverSession,
                                                            const std::string &url,
                                                            const std::string &method)
{
    // The server will call this function when an AbstractServerSession managed to decode the request's url & method
    // If the plugin can handle them it should return an AbstractServiceSession instance, otherwise nullptr
}

PLUGIN_EXPORT void destoryPlugin()
{
// This function is called by the server when it closes. The plugin should wait in this function until it finishes the clean up.
}
{/code}

  Only "createSession" is required, "initPlugin" and "destoryPlugin" are called only if they are found
*/

/// The server calls this function when it loads the plugin
using InitPluginType = bool (*)(const std::string &);

/// The server calls this function to get the plugin order
using PluginOrder = uint32_t (*)();

/// The server calls this function when it needs to create a new session
using CreateSessionType = std::shared_ptr<AbstractServiceSession> (*)(AbstractServerSession *, const std::string &, const std::string &);

/// The server calls this function when it destoyes the plugins
using DestoryPluginType = void (*)();

} // namespace Getodac
