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
*/

#include "serverplugin.h"
#include "serverlogger.h"

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
ServerPlugin::ServerPlugin(const std::string &path, const std::string &confDir)
{
    TRACE(server_logger) << "ServerPlugin loading: " << path << " confDir:" << confDir;
    int flags = RTLD_NOW | RTLD_LOCAL;
#if !defined(__SANITIZE_THREAD__) && !defined(__SANITIZE_ADDRESS__)
    flags |= RTLD_DEEPBIND;
#endif
    m_handler = std::shared_ptr<void>(dlopen(path.c_str(), flags), [](void *ptr) {
        if (ptr) {
            auto destroy = Dracon::DestoryPluginType(dlsym(ptr, "destory_plugin"));
            if (destroy)
                destroy();
            dlclose(ptr);
        }
    });

    if (!m_handler)
        throw std::runtime_error{dlerror()};

    auto init = Dracon::InitPluginType(dlsym(m_handler.get(), "init_plugin"));
    if (init && !init(confDir))
        throw std::runtime_error{"initPlugin failed"};

    createSession = Dracon::CreateSessionType(dlsym(m_handler.get(), "create_session"));
    if (!createSession)
        throw std::runtime_error{"Can't find create_session function"};

    auto order = Dracon::PluginOrder(dlsym(m_handler.get(), "plugin_order"));
    if (!order)
        throw std::runtime_error{"Can't find plugin_order function"};
    m_order = order();
}

/*!
 * \brief ServerPlugin::ServerPlugin
 *
 * \param createSession function pointer
 */
ServerPlugin::ServerPlugin(Dracon::CreateSessionType funcPtr, uint32_t order)
 : createSession(funcPtr)
 , m_order(order)
{}

} // namespace Getodac
