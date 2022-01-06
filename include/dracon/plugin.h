/*
    Copyright (C) 2022, BogDan Vatra <bogdan@kde.org>

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

#include <functional>
#include <string>

namespace Dracon {
class AbstractStream;
class Request;

#define PLUGIN_EXPORT extern "C" __attribute__ ((visibility("default")))

/*!
 Every plugin must implement the following functions as PLUGIN_EXPORT functions:

{code}
PLUGIN_EXPORT bool init_plugin(const std::string &confDir)
{
  // This function is called by the server after it loads the plugin
}

PLUGIN_EXPORT uint32_t plugin_order()
{
    // The server calls this function to get the plugin order
}

PLUGIN_EXPORT HttpSession create_session(const dracon::request& req)
{
    // The server will call this function to create a session for the provided request object
    // If the plugin can handle them it should return an HttpSession object, otherwise {}
}

PLUGIN_EXPORT void destory_plugin()
{
// This function is called by the server when it closes. The plugin should wait in this function until it finishes the clean up.
}
{/code}

  Only "create_session" and "plugin_order" are required, "init_plugin" and "destory_plugin" are called only if they are found
*/

/// The server calls this function when it loads the plugin
using InitPluginType = bool (*)(const std::string &);

/// The server calls this function to get the plugin order
using PluginOrder = uint32_t (*)();

/// The server calls this function when it needs to create a new session
using HttpSession = std::function<void(Dracon::AbstractStream&, Dracon::Request&)>;
using CreateSessionType = HttpSession (*)(const Dracon::Request&);

/// The server calls this function when it destoyes the plugins
using DestoryPluginType = void (*)();

} // namespace dracon
