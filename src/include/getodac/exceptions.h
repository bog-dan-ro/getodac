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

#ifndef EXCEPTIONS_H
#define EXCEPTIONS_H

#include <stdexcept>

namespace Getodac {

/*!
 * \brief The response_error class
 */
class response_status_error : public std::runtime_error
{
public:
    explicit response_status_error(int statusCode, const std::string &errorMessage)
        : std::runtime_error(errorMessage), m_statusCode(statusCode) {}
    explicit response_status_error(int statusCode, const char *errorMessage)
        : std::runtime_error(errorMessage), m_statusCode(statusCode) {}

    /*!
     * \brief statusCode
     * \return returns the http status code
     */
    int statusCode() const { return m_statusCode; }

private:
    int m_statusCode;
};


/*!
 * \brief The broken_pipe_error class
 *
 * Used by AbstractServerSession to throw a broken_pipe_error
 */
class broken_pipe_error : public std::runtime_error
{
public:
  explicit broken_pipe_error(const std::string& __arg)
        : std::runtime_error(__arg){}
  explicit broken_pipe_error(const char* __arg)
        : std::runtime_error(__arg){}
};

/*!
 * \brief The segmentation_fault_error class
 *
 * The server converts any SIGSEGV signals into an exception
 */
class segmentation_fault_error : public std::runtime_error
{
public:
  explicit segmentation_fault_error(const std::string& __arg)
        : std::runtime_error(__arg){}
  explicit segmentation_fault_error(const char* __arg)
        : std::runtime_error(__arg){}
};

/*!
 * \brief The floating_point_error class
 *
 * The server converts any SIGFPE signals into an exception
 */
class floating_point_error : public std::runtime_error
{
public:
  explicit floating_point_error(const std::string& __arg)
        : std::runtime_error(__arg){}
  explicit floating_point_error(const char* __arg)
        : std::runtime_error(__arg){}
};

} // namespace Getodac

#endif // EXCEPTIONS_H
