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

#include "server_plugin.h"

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
ServerPlugin::ServerPlugin(const std::string &path)
{
    m_handler = std::shared_ptr<void>(dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL), [](void *ptr){
        if (ptr) {
            DestoryPluginType destroy = (DestoryPluginType)dlsym(ptr, "destoryPlugin");
            if (destroy)
                destroy();
            dlclose(ptr);
        }
    });

    if (!m_handler)
        throw std::runtime_error{"Can't open " + path};
    try {
        InitPluginType init = (InitPluginType) dlsym(m_handler.get(), "initPlugin");
        if (init && !init())
            throw std::runtime_error{"initPlugin failed"};

        createSession = (CreateSessionType)dlsym(m_handler.get(), "createSession");

        if (!createSession)
            throw std::runtime_error{dlerror()};
    } catch (...) {
        dlclose(m_handler.get());
        throw;
    }
}

/*!
 * \brief ServerPlugin::ServerPlugin
 *
 * \param createSession function pointer
 */
ServerPlugin::ServerPlugin(CreateSessionType funcPtr)
 : createSession(funcPtr)
{
}

ServerPlugin::~ServerPlugin()
{}

} // namespace Getodac
