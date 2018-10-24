/*
    Copyright (C) 2018, BogDan Vatra <bogdan@kde.org>

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

template <typename ReturnType, typename ...Args> class RESTfullResource;

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
using RESTfullResourceMethodCreator = std::function<ReturnType(ParsedUrl parsedUrl, Args ...args)>;

template <typename ReturnType, typename ...Args>
class RESTfullResource
{
public:

    /*!
     * \brief RESTfullResource
     *  A RESTfull resource node. This type is useful to create complex routes for
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
    explicit RESTfullResource(const std::string &resource = {})
        : d(std::make_shared<RESTfullResourceData>())
    {
          d->resource = resource;
    }

    /*!
     * \brief addSubResource
     * \param res
     * \return this
     */
    RESTfullResource &addSubResource(RESTfullResource<ReturnType, Args...> res)
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
    RESTfullResource &addMethodCreator(std::string method, RESTfullResourceMethodCreator<ReturnType, Args...> creator)
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
    ReturnType create(const std::string &url, const std::string &method, Args ...args) const
    {
        if (url.size() <= d->resource.size() ||
                std::memcmp(url.c_str(), d->resource.c_str(), d->resource.size())) {
            return {};
        }

        ReturnType ret;
        // Search for ?
        ParsedUrl parsedUrl;
        auto &queryStrings = parsedUrl.queryStrings;
        auto qpos = findInSubstr(url, 0, url.size(), '?');
        if (qpos != std::string::npos) {
            // time to parse the queryStrings
            // the parser only supports key1=value1&key2=value2... format
            ++qpos;
            for (const auto &kvPair : split(url, '&', qpos, url.size() - qpos)) {
                auto kv = split(url, '=', kvPair.first, kvPair.second);
                switch (kv.size()) {
                case 1:
                    queryStrings.emplace_back(std::make_pair(unEscapeUrl(url.substr(kv[0].first, kv[0].second)), ""));
                    break;
                case 2:
                    queryStrings.emplace_back(std::make_pair(unEscapeUrl(url.substr(kv[0].first, kv[0].second)),
                                                                        unEscapeUrl(url.substr(kv[1].first, kv[1].second))));
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
            if (subResource.createSubResource(ret, method, url, pos, qpos, parsedUrl, std::forward<Args>(args)...))
                return ret;
        }
        return {};
    }

    bool operator==(const RESTfullResource<ReturnType, Args...> &other) const
    {
        return d == other.d;
    }


protected:
    bool createSubResource(ReturnType &ret, const std::string &method, const std::string &url, std::string::size_type pos, std::string::size_type queryPos, ParsedUrl partialParsedUrl, Args ...args) const
    {
        if (d->resource.empty() || (url.size() >= d->resource.size() + pos
                                   && std::memcmp(url.c_str() + pos, d->resource.c_str(), d->resource.size()) == 0)) {

            auto nextPos = findInSubstr(url, pos, queryPos - pos, '/');
            auto data = nextPos == std::string::npos ? url.substr(pos, queryPos - pos)
                                                     : url.substr(pos, nextPos - pos);

            // Placeholder?
            if (d->resource.empty())
                partialParsedUrl.resources.back().second = std::move(unEscapeUrl(data));
            else
                partialParsedUrl.resources.push_back({std::move(data), {}});

            if (nextPos == std::string::npos) {
                auto methodIt = d->methods.find(method);
                if (methodIt == d->methods.end())
                    return false;

                partialParsedUrl.allButOPTIONSNodeMethods = d->allMethods;
                ret = std::move(methodIt->second(partialParsedUrl, args...));
                return true;
            }

            for (const auto &subResource : d->subResources)
                if (subResource.createSubResource(ret, method, url, nextPos + 1, queryPos, partialParsedUrl, args...))
                    return true;
        }
        return false;
    }

protected:
    struct RESTfullResourceData {
        std::string allMethods;
        std::string resource;
        std::unordered_map<std::string, RESTfullResourceMethodCreator<ReturnType, Args...>> methods;
        std::vector<RESTfullResource<ReturnType, Args...>> subResources;
    };
    std::shared_ptr<RESTfullResourceData> d;
};

} // namespace Getodac
