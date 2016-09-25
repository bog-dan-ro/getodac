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

#ifndef ABSTRACT_SERVER_SESSION_H
#define ABSTRACT_SERVER_SESSION_H

#include <stdint.h>
#include <sys/uio.h>
#include <string>
#include <stdexcept>

#include <boost/coroutine/symmetric_coroutine.hpp>

namespace Getodac {

enum {
    ChuckedData = UINT64_MAX
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
     * \brief peerAddress
     * \return the peer address structure
     */
    virtual const struct sockaddr_storage& peerAddress() const = 0;

    /*!
     * \brief isSecuredConnection
     * \return true if this is a SSL connection
     */
    virtual bool isSecuredConnection() const { return false; }


    /*!
     * \brief Yield
     *
     * An object of this type is used by write & writev methods to yield the execution
     * of the current context until they manage to write all the data
     */
    using Yield = boost::coroutines::symmetric_coroutine<Action>::yield_type;

public:
    virtual ~AbstractServerSession() {}

    /*!
     * \brief responseStatus
     *
     * This is the first function that a ServiceSession must call
     *
     * \param code the HTTP response status code. Check https://www.w3.org/Protocols/rfc2616/rfc2616-sec10.html for the complete list
     */
    virtual void responseStatus(uint32_t code) = 0;

    /*!
     * \brief responseHeader
     *
     * Appends a response header
     *
     * \param field string
     * \param value string
     */
    virtual void responseHeader(const std::string &field, const std::string &value) = 0;

    /*!
     * \brief responseEndHeader
     *
     * Ends the response headers. After this function call the ServiceSession should start to write data
     * or to call responseComplete.
     *
     * \param contentLenght the content lenght in bytes or ChuckedData for a chuncked transfer
     * \param keepAliveSeconds number of seconds to keep the connection alive
     * \param continousWrite true means we'll use edge-triggered write notifications, this means that the
     *                       service session must fill the *entire* write buffer to get another notification.
     *                       false means that it will be called every time when the kernel consumes the written buffer.
     *                       In other word use *continousWrite = false* when you have all the data for response
     *                       otherwise use *continousWrite = true*
     */
    virtual void responseEndHeader(uint64_t contentLenght, uint32_t keepAliveSeconds = 10, bool continousWrite = false) = 0;

    /*!
     * \brief write
     *
     * Writes size bytes from buffer to the response and waits until all the data is written.
     * On errors it will throw an exception
     *
     * \param yield object used to yield the execution until all the data is written
     * \param buffer to write
     * \param size in byte of the buffer
     */
    virtual void write(Yield &yield, const void *buffer, size_t size) = 0;

    /*!
     * \brief writev
     *
     * Writes count vec to the response and waits until all the data is written.
     * On errors it will throw an exception
     *
     * \param yield object used to yield the execution until all the data is written
     * \param vec the vector to write
     * \param count the number of vector elements
     */
    virtual void writev(Yield &yield, iovec *vec, size_t count) = 0;

    /*!
     * \brief responseComplete
     *
     * After this function is called, AbstractServiceSession should be ready to be destroyed immediately
     */
    virtual void responseComplete() = 0;
};

} // namespace Getodac
#endif // ABSTRACT_SERVER_SESSION_H
