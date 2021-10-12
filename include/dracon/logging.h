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

#include <boost/log/attributes.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/sources/severity_logger.hpp>
#include <boost/utility/string_view.hpp>

#define LOG BOOST_LOG_SEV

struct DummyStream
{
    template<typename T>
    inline DummyStream &operator << (const T &) {return *this;}
};

/// Multi Thread Severity Logger
using SeverityLoggerMt = boost::log::sources::severity_logger_mt<boost::log::trivial::severity_level>;
/// Single Thread Severity Logger
using SeverityLogger = boost::log::sources::severity_logger<boost::log::trivial::severity_level>;

template <typename T = SeverityLoggerMt>
class TaggedLogger : public T
{
public:
    TaggedLogger(boost::string_view value)
    {
        T::add_attribute("Tag", boost::log::attributes::make_constant(value));
    }
};

#ifdef ENABLE_TRACE_LOG
# define TRACE(logger) LOG(logger, boost::log::trivial::trace) << __PRETTY_FUNCTION__ << " : "
#else
# define TRACE(logger) DummyStream{}
#endif

#ifdef ENABLE_DEBUG_LOG
#define DEBUG(logger) LOG(logger, boost::log::trivial::debug) << __PRETTY_FUNCTION__ << " : "
#else
# define DEBUG(logger)  DummyStream{}
#endif

#define INFO(logger) LOG(logger, boost::log::trivial::info) << __PRETTY_FUNCTION__ << " : "
#define WARNING(logger) LOG(logger, boost::log::trivial::warning) << __PRETTY_FUNCTION__ << " : "
#define ERROR(logger) LOG(logger, boost::log::trivial::error) << __PRETTY_FUNCTION__ << " : "
#define FATAL(logger) LOG(logger, boost::log::trivial::fatal) << __PRETTY_FUNCTION__ << " : "
