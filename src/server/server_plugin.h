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
*/

#pragma once

#include <memory>
#include <dracon/plugin.h>

namespace Getodac {

/*!
 * \brief The ServerPlugin class
 *
 * This class is used to load plugins
 */
class server_plugin
{
public:
    explicit server_plugin(const std::string &path, const std::string &confDir);
    explicit server_plugin(dracon::CreateSessionType funcPtr, uint32_t order);
    dracon::CreateSessionType create_session;
    uint32_t order() const { return m_order; }

private:
    std::shared_ptr<void> m_handler;
    uint32_t m_order = 0;
};

} // namespace Getodac
