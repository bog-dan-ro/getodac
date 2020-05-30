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

template <typename ReturnType, typename ...Args> class RESTfulResource;

using Resources = std::vector<std::pair<std::string, std::string>>;
using QueryStrings = std::vector<std::pair<std::string, std::string>>;

struct ParsedUrl {
    /*!
     * \brief The parsed resources.
     * It's a key value pair vector. The order is the same as the URL order
     */
    Resources resources;

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

    bool operator ==(const ParsedUrl &other) const
    {
        return allButOPTIONSNodeMethods == other.allButOPTIONSNodeMethods &&
                resources == other.resources && queryStrings == other.queryStrings;
    }
};

template <typename ReturnType, typename ...Args>
using RESTfulResourceMethodCreator = std::function<ReturnType(ParsedUrl parsedUrl, Args ...args)>;

template <typename ReturnType, typename ...Args>
class RESTfulResource
{
public:

    /*!
     * \brief RESTfulResource
     *  A RESTful resource node. This type is useful to create complex routes for
     *  REST API.
     *
     * \param resource. for the root node, this parameter contains the base url
     *  for all the other resources (e.g /api/v1/) and it should end with an /.
     *  An empty string means it's a resource id placeholder and its value it
     *  will be added to it's parent resource.
     *
     * \example
     *
     */
    explicit RESTfulResource(const std::string &resource = {})
        : d(std::make_shared<RESTfulResourceData>())
    {
          d->resource = resource;
    }

    /*!
     * \brief addSubResource
     * \param res
     * \return this
     */
    RESTfulResource &addSubResource(RESTfulResource<ReturnType, Args...> res)
    {
        if (!d->subResources.empty()) {
            // Do some sanity check
            if (res.d->resource.empty() && d->subResources.back().d->resource.empty())
                throw std::runtime_error{"There can be only one subresource placeholder"};
            if (!res.d->resource.empty() && d->subResources.back().d->resource.empty()) {
                d->subResources.emplace(d->subResources.begin() + d->subResources.size() - 1, std::move(res));
                return *this;
            }
        }
        d->subResources.emplace_back(std::move(res));
        return *this;
    }

    /*!
     * \brief addMethodCreator
     * \param method
     * \param creator
     * \return this
     */
    RESTfulResource &addMethodCreator(std::string method, RESTfulResourceMethodCreator<ReturnType, Args...> creator)
    {
        if (d->methods.find(method) == d->methods.end()) {
            if (method != "OPTIONS")
                d->allMethods += d->allMethods.empty() ? method : ", " + method;
            d->methods.emplace(std::move(method), std::move(creator));
        } else {
            d->methods[method] = std::move(creator);
        }
        return *this;
    }

    /*!
     * \brief create
     * \param url
     * \param method
     * \param args
     * \return ReturnType
     */
    ReturnType create(const std::string_view &url, const std::string &method, Args ...args) const
    {
        if (url.size() <= d->resource.size() ||
                std::memcmp(url.data(), d->resource.c_str(), d->resource.size())) {
            return {};
        }

        ReturnType ret;
        // Search for ?
        ParsedUrl parsedUrl;
        auto &queryStrings = parsedUrl.queryStrings;
        auto qpos = url.find('?');
        if (qpos != std::string::npos) {
            // time to parse the queryStrings
            // the parser only supports key1=value1&key2=value2... format
            ++qpos;
            for (const auto &kvPair : split(url.substr(qpos), '&')) {
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
            --qpos;
        } else {
            qpos = url.size();
        }

        auto pos = d->resource.size();

        for (const auto &subResource : d->subResources) {
            if (subResource.createSubResource(ret, method, url.substr(pos, qpos - pos), parsedUrl, std::forward<Args>(args)...))
                return ret;
        }
        return {};
    }

    bool operator==(const RESTfulResource<ReturnType, Args...> &other) const
    {
        return d == other.d;
    }


protected:
    bool createSubResource(ReturnType &ret, const std::string &method, const std::string_view &url, ParsedUrl partialParsedUrl, Args ...args) const
    {
        if (d->resource.empty() || (url.size() >= d->resource.size()
                                   && std::memcmp(url.data(), d->resource.c_str(), d->resource.size()) == 0)) {

            auto nextPos = url.find('/');
            auto data = nextPos == std::string::npos ? url : url.substr(0, nextPos);

            // Placeholder?
            if (d->resource.empty())
                partialParsedUrl.resources.back().second = std::move(unEscapeUrl(data));
            else
                partialParsedUrl.resources.push_back({std::string{data}, {}});

            if (nextPos == std::string::npos) {
                auto methodIt = d->methods.find(method);
                if (methodIt == d->methods.end())
                    return false;

                partialParsedUrl.allButOPTIONSNodeMethods = d->allMethods;
                ret = std::move(methodIt->second(partialParsedUrl, args...));
                return true;
            }

            for (const auto &subResource : d->subResources)
                if (subResource.createSubResource(ret, method, url.substr(nextPos + 1), partialParsedUrl, args...))
                    return true;
        }
        return false;
    }

protected:
    struct RESTfulResourceData {
        std::string allMethods;
        std::string resource;
        std::unordered_map<std::string, RESTfulResourceMethodCreator<ReturnType, Args...>> methods;
        std::vector<RESTfulResource<ReturnType, Args...>> subResources;
    };
    std::shared_ptr<RESTfulResourceData> d;
};

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

    bool operator == (const std::string_view &route) {
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

    RouteParts routeParts(const std::string_view &route) const
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
    RESTfulRoute(const std::string_view &route)
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
    RESTfulRoutePtr createRoute(const std::string_view &route)
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
    ReturnType createHandler(const std::string_view &url, const std::string &method, Args ...args) const
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
