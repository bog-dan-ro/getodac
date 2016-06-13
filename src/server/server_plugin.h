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

#ifndef SERVER_PLUGIN_H
#define SERVER_PLUGIN_H

#include <getodac/abstract_service_session.h>

namespace Getodac {

/*!
 * \brief The ServerPlugin class
 *
 * This class is used to load plugins
 */
class ServerPlugin
{
public:
    explicit ServerPlugin(const std::string &path, const std::__cxx11::string &confDir);
    explicit ServerPlugin(CreateSessionType funcPtr, uint32_t order);
    ~ServerPlugin();
    CreateSessionType createSession;
    uint32_t order() const { return m_order; }

private:
    std::shared_ptr<void> m_handler;
    uint32_t m_order = 0;
};

} // namespace Getodac

#endif // SERVER_PLUGIN_H
