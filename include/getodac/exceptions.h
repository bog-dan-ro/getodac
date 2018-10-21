/*
    Copyright (C) 2018, BogDan Vatra <bogdan@kde.org>

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

#include <stdexcept>
#include <unordered_map>

namespace Getodac {

/*!
 * \brief The response_error class
 */
class ResponseStatusError : public std::runtime_error
{
public:
    using HeadersData = std::unordered_map<std::string, std::string>;
    ResponseStatusError(int statusCode, const std::string &errorMessage = {}, HeadersData headers = {})
        : std::runtime_error(errorMessage)
        , m_statusCode(statusCode)
        , m_headers(std::move(headers))
    {}

    /*!
     * \brief statusCode
     * \return returns the http status code
     */
    int statusCode() const { return m_statusCode; }

    /*!
     * \brief headers
     * \return returns the headers
     */
    const HeadersData headers() const {return m_headers; }

private:
    int m_statusCode;
    HeadersData m_headers;
};


/*!
 * \brief The segmentation_fault_error class
 *
 * The server converts any SIGSEGV signals into an exception
 */
class SegmentationFaultError : public std::runtime_error
{
public:
  explicit SegmentationFaultError(const std::string& __arg)
        : std::runtime_error(__arg){}
  explicit SegmentationFaultError(const char* __arg)
        : std::runtime_error(__arg){}
};

/*!
 * \brief The floating_point_error class
 *
 * The server converts any SIGFPE signals into an exception
 */
class FloatingPointError : public std::runtime_error
{
public:
  explicit FloatingPointError(const std::string& __arg)
        : std::runtime_error(__arg){}
  explicit FloatingPointError(const char* __arg)
        : std::runtime_error(__arg){}
};

} // namespace Getodac
