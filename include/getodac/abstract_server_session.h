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

#include <openssl/ssl.h>
#include <stdint.h>
#include <sys/eventfd.h>
#include <sys/socket.h>
#include <sys/uio.h>

#include <chrono>
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace Getodac {

enum {
    ChunkedData = UINT64_MAX
};

using HeadersData = std::unordered_map<std::string, std::string>;

struct ResponseHeaders
{
    /*!
     * \brief status. The HTTP response status code.
     * Check https://www.w3.org/Protocols/rfc2616/rfc2616-sec10.html for the complete list
     */
    uint32_t status = 200;

    /*!
     * \brief headers. The HTTP response headers.
     */
    HeadersData headers;

    /*!
     * \brief contentLenght. The content lenght in bytes or Getodac::ChunkedData for a chunked transfer.
     */
    uint64_t contentLength = 0;

    /*!
     * \brief keepAlive. The number of seconds to keep the connection alive if it was requested.
     */
    std::chrono::seconds keepAlive{10};
};

/*!
 * \brief The AbstractServerSession class
 */
class AbstractServerSession {
public:
    enum class Action {
        Continue,
        Timeout,
        Quit
    };

    /*!
     * \brief Yield
     *
     * An object of this type is used by write & writev methods to yield the execution
     * of the current context until they manage to write all the data
     */
    struct Yield {
        /*!
         * \brief operator ()
         *
         * Yield the execution until next write event
         */
        virtual void operator()() = 0;
        virtual Action get() = 0;
    };

    /*!
     * \brief The Wakeupper class
     *
     * This class it's useful to wake up the session, from a worker thread, to write the processed buffer
     */
    class Wakeupper {
    public:
        inline bool wakeUp() const {return eventfd_write(m_fd, m_value) == 0;}
        ~Wakeupper() = default;

    private:
        friend class ServerSession;
        Wakeupper(int fd, uint64_t value)
            : m_fd(fd)
            , m_value(value)
        {}
        int m_fd;
        uint64_t m_value;
    };

public:
    virtual ~AbstractServerSession() = default;

    /*!
     * \brief wakeuppper
     * \return Wakeupper object
     */
    virtual Wakeupper wakeuppper() const = 0;

    /*!
     * \brief write
     * Writes the only the headers data
     */
    virtual void write(Yield &yield, const ResponseHeaders &response) = 0;

    /*!
     * \brief write
     * Writes the headers and the data in one go.
     * This function tries to minimize the syscalls
     * by using writev to write the headers and the data.
     */
    virtual void write(Yield &yield, const ResponseHeaders &response, std::string_view data) = 0;

    /*!
     * \brief writev
     * Same as above, but instead of one data, it accepts a vector of datas
     */
    virtual void writev(Yield &yield, const ResponseHeaders &response, iovec *vec, size_t count) = 0;

    /*!
     * \brief write
     *
     * Writes the buffer to the response and waits until all the data is written.
     * On errors it will throw an exception.
     *
     * If the response headers were not written before, it throws an error immediately.
     *
     * \param yield object used to yield the execution until all the data is written
     * \param data the buffer to write
     */
    virtual void write(Yield &yield, std::string_view data) = 0;

    /*!
     * \brief writev
     *
     * Writes count vec to the response and waits until all the data is written.
     * On errors it will throw an exception
     *
     * If the response headers were not written before, it throws an error immediately.
     *
     * \param yield object used to yield the execution until all the data is written
     * \param vec the vector to write
     * \param count the number of vector elements
     */
    virtual void writev(Yield &yield, iovec *vec, size_t count) = 0;

    /*!
     * \brief peerAddress
     * \return the peer address structure
     */
    virtual const sockaddr_storage& peerAddress() const = 0;

    /*!
     * \brief isSecuredConnection
     * \return true if this is a SSL connection
     */
    virtual bool isSecuredConnection() const { return false; }

    /*!
     * \brief verifyPeer initiate the peer verification procedure
     * \param caFiles the clients ca files to verify
     */
    virtual void verifyPeer(const std::string &caFile = {}) {(void)caFile;}

    /*!
     * \brief getPeerCertificate
     * \return the peer certificate if any
     */
    virtual X509* getPeerCertificate() const { return nullptr; }

    /*!
     * \brief sendBufferSize
     * \return the socket send buffer size in bytes
     */
    virtual int sendBufferSize() const = 0;

    /*!
     * \brief setSendBufferSize
     *
     * Set's the socket sending buffer size
     *
     * \param size in bytes
     * \return true on success
     */
    virtual bool setSendBufferSize(int size) = 0;

    /*!
     * \brief receiveBufferSize
     * \return the socket receive buffer size in bytes
     */
    virtual int receiveBufferSize() const = 0;

    /*!
     * \brief setReceiveBufferSize
     *
     * Set's the socket receiving buffer size
     *
     * \param size in bytes
     * \return true on success
     */
    virtual bool setReceiveBufferSize(int size) = 0;

    /*!
     * \brief setTimeout
     *
     * Set's a new session timeout
     *
     * \param ms 0 means it will never timeout
     */
    virtual void setTimeout(const std::chrono::milliseconds &ms) = 0;

    /*!
     * \brief responseHeadersString
     *
     * Returns a string with formated headers ready for seding to socket
     */
    virtual std::string responseHeadersString(const ResponseHeaders &hdrs) = 0;
};

} // namespace Getodac
