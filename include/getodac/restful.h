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
#include <getodac/utils.h>

namespace Getodac {
class AbstractServerSession;

using QueryStrings = std::vector<std::pair<std::string, std::string>>;

struct ParsedRoute
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

    bool operator ==(const ParsedRoute &other) const
    {
        return allButOPTIONSNodeMethods == other.allButOPTIONSNodeMethods &&
                capturedResources == other.capturedResources && queryStrings == other.queryStrings;
    }
};

template <typename ReturnType, typename ...Args>
using RESTfulRouteMethodHandler = std::function<ReturnType(ParsedRoute parsedRoute, Args ...args)>;

template <typename ReturnType, typename ...Args> class RESTfulRouter;

template <typename ReturnType, typename ...Args>
class RESTfulRoute
{
public:
    /*!
     * \brief addMethodCreator
     * \param method
     * \param creator
     * \return
     */
    RESTfulRoute &addMethodHandler(std::string method, RESTfulRouteMethodHandler<ReturnType, Args...> creator)
    {
        if (m_methods.find(method) == m_methods.end()) {
            if (method != "OPTIONS")
                m_allMethods += m_allMethods.empty() ? method : ", " + method;
            m_methods.emplace(std::move(method), std::move(creator));
        } else {
            m_methods[method] = std::move(creator);
        }
        return *this;
    }

    bool operator == (std::string_view route) {
        auto other = routeParts(route);
        if (other.size() != m_routeParts.size())
            return false;
        for (int i = 0; i < other.size(); ++i) {
            if (other[i].first != m_routeParts[i].first ||
                    other[i].second != m_routeParts[i].second) {
                return false;
            }
        }
        return true;
    }
protected:
    using RouteParts = std::vector<std::pair<bool, std::string>>;
    RouteParts m_routeParts;
    std::unordered_map<std::string, RESTfulRouteMethodHandler<ReturnType, Args...>> m_methods;
    std::string m_allMethods;

private:
    template <typename T, typename ...A>
    friend class RESTfulRouter;

    RouteParts routeParts(std::string_view route) const
    {
        RouteParts res;
        auto routeParts = split(route, '/');
        for (const auto &part : routeParts) {
            if (part.size() < 2)
                throw std::runtime_error{"Invalid route"};
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
    RESTfulRoute(std::string_view route)
        : m_routeParts(std::move(routeParts(route)))
    {}

    using Captures = std::unordered_map<std::string, std::string>;
    std::optional<std::pair<Captures, RESTfulRouteMethodHandler<ReturnType, Args...>>>
    createHandler(const SplitVector &urlParts, const std::string &method) const
    {
        if (urlParts.size() != m_routeParts.size())
            return {};
        Captures captures;
        for (size_t i = 0; i < urlParts.size(); ++i) {
            const auto &part = m_routeParts[i];
            if (part.first) {
                captures[part.second] = urlParts[i];
             } else if (part.second != urlParts[i]) {
                return  {};
            }
        }
        auto methodIt = m_methods.find(method);
        if (methodIt == m_methods.end())
            throw ResponseStatusError{405, {}, {{"Allow", m_allMethods}}};
        return std::make_optional(std::make_pair(std::move(captures), methodIt->second));
    }
};

/*!
 * \brief The RESTfulRouter class
 */
template <typename ReturnType, typename ...Args>
class RESTfulRouter
{
    using RESTfulRoutePtr = std::shared_ptr<RESTfulRoute<ReturnType, Args...>>;
public:
    RESTfulRouter(std::string baseUrl = {})
    {
        auto baseUrlParts = split(baseUrl, '/');
        for (const auto &part : baseUrlParts)
            m_baseUrl.emplace_back(std::string{part});
    }
    /*!
     * \brief ceateRoute
     * \param route
     * \return
     */
    RESTfulRoutePtr createRoute(std::string_view route)
    {
        for (auto rt : m_routes)
            if (*rt == route)
                return rt;
        return m_routes.emplace_back(RESTfulRoutePtr{new RESTfulRoute<ReturnType, Args...>{route}});
    }

    /*!
     * \brief createHandle parse the given \a url and \a method and if they match
     * with a route, it creates and returns a handler. Otherwise it returns {}
     */
    ReturnType createHandler(std::string_view url, const std::string &method, Args ...args) const
    {
        auto qpos = url.find('?');
        auto resources = split(url.substr(0, qpos), '/');
        if (resources.size() < m_baseUrl.size() + 1)
            return {};
        for (size_t i = 0; i < m_baseUrl.size(); ++i)
            if (resources[i] != m_baseUrl[i])
                return {};
        resources.erase(resources.begin(), resources.begin() + m_baseUrl.size());
        for (const auto &route : m_routes) {
            if (auto handle = route->createHandler(resources, method)) {
                ParsedRoute parsedRoute;
                parsedRoute.allButOPTIONSNodeMethods = route->m_allMethods;
                parsedRoute.capturedResources = std::move(handle->first);
                if (qpos != std::string::npos) {
                    auto &queryStrings = parsedRoute.queryStrings;
                    for (const auto &kvPair : split(url.substr(qpos + 1), '&')) {
                        auto kv = split(kvPair, '=');
                        switch (kv.size()) {
                        case 1:
                            queryStrings.emplace_back(std::make_pair(unEscapeUrl(kv[0]), ""));
                            break;
                        case 2:
                            queryStrings.emplace_back(std::make_pair(unEscapeUrl(kv[0]),
                                                                                unEscapeUrl(kv[1])));
                            break;
                        default:
                            throw ResponseStatusError{400, "Invalid query strings"};
                        }
                    }
                }
                return handle->second(parsedRoute, args...);
            }
        }
        return {};
    }

protected:
    std::vector<std::string> m_baseUrl;
    std::vector<RESTfulRoutePtr> m_routes;
};
} // namespace Getodac
