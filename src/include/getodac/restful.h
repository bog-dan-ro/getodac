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

#ifndef RESTFUL_H
#define RESTFUL_H

#include <cstring>
#include <functional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <getodac/exceptions.h>
#include <getodac/utils.h>

namespace Getodac {
class AbstractServerSession;

struct Resource {
    std::string name;
    std::string value;
    std::vector<std::pair<std::string, std::string>> queryStrings;
};

using Resources = std::vector<Resource>;

/*!
 * The RESTful class
 */
template <typename Return = void, typename Param = AbstractServerSession*>
class RESTful
{
public:
    using Func = std::function<Return(Param param, Resources &&resources)>;
    RESTful(const std::string &urlPrefix) : m_urlPrefix(urlPrefix) {}

    /*!
     * \brief setMethodCallback
     *
     * Sets the method callback and parameters
     *
     * \param method to set, most common are GET, POST, PUT, PATCH and DELETE
     * \param callback function
     * \param parameters accepted by this urlPrefix
     */
    void setMethodCallback(const std::string &method, const Func &callback, std::initializer_list<std::string> resources)
    {
        m_methods.emplace(std::make_pair(method, std::make_pair(callback, resources)));
    }

    inline bool canHanldle(const std::string &url, const std::string &method)
    {
        return url.size() >= m_urlPrefix.size() &&
                std::memcmp(url.c_str(), m_urlPrefix.c_str(), m_urlPrefix.size()) == 0 &&
                m_methods.find(method) != m_methods.end();
    }

    /*!
     * \brief parse
     *
     * Parses the givem url and executes the method callback set by setMethodCallback and it returns the T value of that method
     *
     * \param url to parse
     * \param method method to execute
     * \return returns
     */
    Return parse(Param param, const std::string &url, const std::string &method)
    {
        if (!canHanldle(url, method))
            throw std::runtime_error("Unhandled url / method");

        const auto &methodPair = m_methods[method];
        auto methodResources = methodPair.second;
        Resources resourses;

        // Split resources
        bool searchForValue =  false;
        for (const auto &resourceChunck : split(url, '/', m_urlPrefix.size(), url.size() - m_urlPrefix.size())) {
            if (searchForValue) {
                resourses.back().value = std::move(unescape(url.substr(resourceChunck.first, resourceChunck.second)));
            } else {
                // Prepare the resource
                Resource resource;

                // Search for ?
                auto qpos = findInSubstr(url, resourceChunck.first, resourceChunck.second, '?');
                if (qpos != std::string::npos)
                    resource.name = std::move(url.substr(resourceChunck.first, qpos - resourceChunck.first));
                else
                    resource.name = std::move(url.substr(resourceChunck.first, resourceChunck.second));

                // the resource name should not be escaped
                auto it = methodResources.find(resource.name);
                if ( it == methodResources.end())
                    throw ResponseStatusError{400, "Unknown resource name \"" + resource.name + "\""};
                methodResources.erase(it);

                if (qpos != std::string::npos) {
                    // time to parse the queryStrings
                    // the parser only supports key1=value1&key2=value2... format
                    ++qpos;
                    for (const auto &kvPair : split(url, '&', qpos, resourceChunck.second - (qpos - resourceChunck.first))) {
                        auto kv = split(url, '=', kvPair.first, kvPair.second);
                        switch (kv.size()) {
                        case 1:
                            resource.queryStrings.emplace_back(std::make_pair(unescape(url.substr(kv[0].first, kv[0].second)), ""));
                            break;
                        case 2:
                            resource.queryStrings.emplace_back(std::make_pair(unescape(url.substr(kv[0].first, kv[0].second)),
                                                                                unescape(url.substr(kv[1].first, kv[1].second))));
                            break;
                        default:
                            throw ResponseStatusError{400, "Invalid query strings"};
                        }
                    }
                }
                resourses.push_back(std::move(resource));
            }
            searchForValue = !searchForValue;
        }
        return methodPair.first(param, std::move(resourses));
    }

private:

    inline char toHex(char ch)
    {
        return isdigit(ch) ? ch - '0' : 10 + tolower(ch) - 'a';
    }

    inline std::string unescape(const std::string &in)
    {
        std::string out;
        out.reserve(in.size());
        for (std::string::size_type i = 0 ; i < in.size(); ++i) {
            switch (in[i]) {
            case '%':
                if (i + 3 < in.size()) {
                                            // it's faster than std::stoi(in.substr(i + 1, 2)) ...
                    out += static_cast<char>(toHex(in[i + 1]) << 4 | toHex(in[i + 2]));
                    i += 2;
                } else {
                    return in;
                }
                break;
            case '+':
                out += ' ';
                break;
            default:
                out += in[i];
                break;
            }
        }
        return out;
    }

private:
    std::unordered_map<std::string, std::pair<Func, std::unordered_set<std::string>>> m_methods;
    std::string m_urlPrefix;
};

} // namespace Getodac

#endif // RESTFUL_H
