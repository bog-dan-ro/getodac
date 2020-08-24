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

    AGPL EXCEPTION:
    The AGPL license applies only to this file itself.

    As a special exception, the copyright holders of this file give you permission
    to use it, regardless of the license terms of your work, and to copy and distribute
    them under terms of your choice.
    If you do any changes to this file, these changes must be published under AGPL.
*/

#pragma once

#include <cstring>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <getodac/exceptions.h>
#include <getodac/http.h>
#include <getodac/plugin.h>
#include <getodac/utils.h>

namespace Getodac {
class AbstractServerSession;

using QueryStrings = std::vector<std::pair<std::string, std::string>>;

struct parsed_route
{
    /*!
     * \brief capturedResources
     * It contains all captured values
     */
    std::unordered_map<std::string, std::string> capturedResources;
    /*!
     * \brief The parsed queryStrings.
     * It's a key value pair vector. The order is the same as the URL order
     */
    QueryStrings queryStrings;

    /*!
     * \brief allNodeMethodsButOPTIONS
     *
     *  All the route node methods but without OPTIONS.
     *  It's alredy formated to send OPTIONS responses.
     */
    std::string allButOPTIONSNodeMethods;

    bool operator ==(const parsed_route &other) const
    {
        return allButOPTIONSNodeMethods == other.allButOPTIONSNodeMethods &&
                capturedResources == other.capturedResources && queryStrings == other.queryStrings;
    }
};

template <typename ReturnType, typename ...Args>
using RESTfulRouteMethodHandler = std::function<ReturnType(parsed_route parsedRoute, Args ...args)>;

template <typename ReturnType, typename ...Args> class restful_router;

template <typename ReturnType, typename ...Args>
class restfull_route
{
public:
    /*!
     * \brief addMethodCreator
     * \param method
     * \param creator
     * \return
     */
    restfull_route &add_method_handler(std::string method, RESTfulRouteMethodHandler<ReturnType, Args...> creator)
    {
        if (m_methods.find(method) == m_methods.end()) {
            if (method != "OPTIONS")
                m_all_methods += m_all_methods.empty() ? method : ", " + method;
            m_methods.emplace(std::move(method), std::move(creator));
        } else {
            m_methods[method] = std::move(creator);
        }
        return *this;
    }

    bool operator == (std::string_view route) {
        auto other = route_parts(route);
        if (other.size() != m_route_parts.size())
            return false;
        for (int i = 0; i < other.size(); ++i) {
            if (other[i].first != m_route_parts[i].first ||
                    other[i].second != m_route_parts[i].second) {
                return false;
            }
        }
        return true;
    }
protected:
    using RouteParts = std::vector<std::pair<bool, std::string>>;
    RouteParts m_route_parts;
    std::unordered_map<std::string, RESTfulRouteMethodHandler<ReturnType, Args...>> m_methods;
    std::string m_all_methods;

private:
    template <typename T, typename ...A>
    friend class restful_router;

    RouteParts route_parts(std::string_view route) const
    {
        RouteParts res;
        auto routeParts = split(route, '/');
        for (const auto &part : routeParts) {
            if (part.size() < 2)
                throw response{400, "Invalid route"};
            if (part.front() == '{' && part.back() == '}')
                res.emplace_back(std::make_pair(true, std::string{part.substr(1, part.size() - 2)}));
            else
                res.emplace_back(std::make_pair(false, std::string{part}));
        }
        return res;
    }

    /*!
     * \brief RESTfulRoute Creates a new RESTful route
     *
     * \param route the route to match, the capture resources must be inside {}
     *               e.g /api/v1/parents/{parent}/children/{child}
     */
    restfull_route(std::string_view route)
        : m_route_parts(std::move(route_parts(route)))
    {}

    using Captures = std::unordered_map<std::string, std::string>;
    std::optional<std::pair<Captures, RESTfulRouteMethodHandler<ReturnType, Args...>>>
    create_handler(const SplitVector &urlParts, const std::string &method) const
    {
        if (urlParts.size() != m_route_parts.size())
            return {};
        Captures captures;
        for (size_t i = 0; i < urlParts.size(); ++i) {
            const auto &part = m_route_parts[i];
            if (part.first) {
                captures[part.second] = urlParts[i];
            } else if (part.second != urlParts[i]) {
                return  {};
            }
        }
        auto method_it = m_methods.find(method);
        if (method_it == m_methods.end())
            throw response{405, {}, {{"Allow", m_all_methods}}};
        return std::make_optional(std::make_pair(std::move(captures), method_it->second));
    }
};

/*!
 * \brief The RESTfulRouter class
 */
template <typename ReturnType, typename ...Args>
class restful_router
{
    using RESTfulRoutePtr = std::shared_ptr<restfull_route<ReturnType, Args...>>;
public:
    restful_router(std::string baseUrl = {})
    {
        auto baseUrlParts = split(baseUrl, '/');
        for (const auto &part : baseUrlParts)
            m_base_url.emplace_back(std::string{part});
    }
    /*!
     * \brief ceateRoute
     * \param route
     * \return
     */
    RESTfulRoutePtr create_route(std::string_view route)
    {
        for (auto rt : m_routes)
            if (*rt == route)
                return rt;
        return m_routes.emplace_back(RESTfulRoutePtr{new restfull_route<ReturnType, Args...>{route}});
    }

    /*!
     * \brief createHandle parse the given \a url and \a method and if they match
     * with a route, it creates and returns a handler. Otherwise it returns {}
     */
    ReturnType create_handler(std::string_view url, const std::string &method, Args ...args) const
    {
        auto qpos = url.find('?');
        auto resources = split(url.substr(0, qpos), '/');
        if (resources.size() < m_base_url.size() + 1)
            return {};
        for (size_t i = 0; i < m_base_url.size(); ++i)
            if (resources[i] != m_base_url[i])
                return {};
        resources.erase(resources.begin(), resources.begin() + m_base_url.size());
        for (const auto &route : m_routes) {
            if (auto handle = route->create_handler(resources, method)) {
                parsed_route parsedRoute;
                parsedRoute.allButOPTIONSNodeMethods = route->m_all_methods;
                parsedRoute.capturedResources = std::move(handle->first);
                if (qpos != std::string::npos) {
                    auto &queryStrings = parsedRoute.queryStrings;
                    for (const auto &kvPair : split(url.substr(qpos + 1), '&')) {
                        auto kv = split(kvPair, '=');
                        switch (kv.size()) {
                        case 1:
                            queryStrings.emplace_back(std::make_pair(unescape_url(kv[0]), ""));
                            break;
                        case 2:
                            queryStrings.emplace_back(std::make_pair(unescape_url(kv[0]),
                                                      unescape_url(kv[1])));
                            break;
                        default:
                            throw response{400, "Invalid query strings"};
                        }
                    }
                }
                return handle->second(parsedRoute, args...);
            }
        }
        return {};
    }

protected:
    std::vector<std::string> m_base_url;
    std::vector<RESTfulRoutePtr> m_routes;
};

using RESTfulRouterType = restful_router<HttpSession>;

template <typename T>
RESTfulRouteMethodHandler<HttpSession> session_handler(T && function)
{
    return [function = std::move(function)](parsed_route &&route) -> HttpSession {
        return std::bind<void>(function, std::move(route), std::placeholders::_1, std::placeholders::_2);
    };
}

} // namespace Getodac
