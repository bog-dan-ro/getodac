/*
    Copyright (C) 2016, BogDan Vatra <bogdan@kde.org>

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

#ifndef SERVICE_SESSION_H
#define SERVICE_SESSION_H

#include "abstract_server_session.h"

#include <sstream>
#include <string>

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
    explicit AbstractServiceSession(AbstractServerSession *serverSession, const std::string &url, const std::string &method)
        : m_serverSession(serverSession)
        , m_url(url)
        , m_method(method)
    {}

    virtual ~AbstractServiceSession() {}

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
     * \param yield object passed to writeResponse, this function will use it to wait until all the data is written
     * \param buf the buffer you want to write
     * \param length the length of the buffer, length = 0 means end of chunked transfer
     */
    void writeChunkedData(AbstractServerSession::Yield &yield, const void *buf, size_t length)
    {
        iovec chunkData[3];
        std::ostringstream chunkHeaderBuf;
        chunkHeaderBuf << std::hex << length << crlf;
        auto chunkHeader = chunkHeaderBuf.str();
        chunkHeaderBuf.str({});
        chunkData[0].iov_base = (void*)chunkHeader.c_str();
        chunkData[0].iov_len = chunkHeader.length();
        chunkData[1].iov_base = (void*)buf;
        chunkData[1].iov_len = length;
        chunkData[2].iov_base = (void*)crlf;
        chunkData[2].iov_len = 2;
        m_serverSession->writev(yield, chunkData, 3);
    }

protected:
    AbstractServerSession *m_serverSession = nullptr;
    std::string m_url;
    std::string m_method;
};

#define PLUGIN_EXPORT extern "C" __attribute__ ((visibility("default")))

/*!
 Every plugin must implement the following functions as PLUGIN_EXPORT functions:

{code}
PLUGIN_EXPORT bool initPlugin()
{
  // This function is called by the server after it loads the plugin
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
typedef bool (*InitPluginType)();

/// The server calls this function when it needs to create a new session
typedef std::shared_ptr<AbstractServiceSession> (*CreateSessionType)(AbstractServerSession *serverSession, const std::string &url, const std::string &method);

/// The server calls this function when it destoyes the plugins
typedef void (*DestoryPluginType)();

} // namespace Getodac

#endif // SERVICE_SESSION_H
