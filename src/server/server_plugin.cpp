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

#include "server_plugin.h"
#include "server_logger.h"

#include <dlfcn.h>

#include <memory>
#include <stdexcept>

namespace Getodac {
/*!
 * \brief ServerPlugin::ServerPlugin
 *
 * Try to load a plugin file
 *
 * \param path to plugin
 */
server_plugin::server_plugin(const std::string &path, const std::string &confDir)
{
    TRACE(serverLogger) << "ServerPlugin loading: " << path << " confDir:" << confDir;
    int flags = RTLD_NOW | RTLD_LOCAL;
#if !defined(__SANITIZE_THREAD__) && !defined(__SANITIZE_ADDRESS__)
    flags |= RTLD_DEEPBIND;
#endif
    m_handler = std::shared_ptr<void>(dlopen(path.c_str(), flags), [](void *ptr) {
        if (ptr) {
            auto destroy = DestoryPluginType(dlsym(ptr, "destory_plugin"));
            if (destroy)
                destroy();
            dlclose(ptr);
        }
    });

    if (!m_handler)
        throw std::runtime_error{dlerror()};

    auto init = InitPluginType(dlsym(m_handler.get(), "init_plugin"));
    if (init && !init(confDir))
        throw std::runtime_error{"initPlugin failed"};

    create_session = CreateSessionType(dlsym(m_handler.get(), "create_session"));
    if (!create_session)
        throw std::runtime_error{"Can't find create_session function"};

    auto order = PluginOrder(dlsym(m_handler.get(), "plugin_order"));
    if (!order)
        throw std::runtime_error{"Can't find plugin_order function"};
    m_order = order();
}

/*!
 * \brief ServerPlugin::ServerPlugin
 *
 * \param createSession function pointer
 */
server_plugin::server_plugin(CreateSessionType funcPtr, uint32_t order)
 : create_session(funcPtr)
 , m_order(order)
{}

server_plugin::~server_plugin() = default;

} // namespace Getodac
