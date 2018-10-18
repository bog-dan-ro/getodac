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

using Resources = std::vector<std::pair<std::string, std::string>>;
using QueryStrings = std::vector<std::pair<std::string, std::string>>;

using ParsedUrl = std::pair<Resources, QueryStrings>;

/*!
 * The RESTful class
 */
template <typename Return, typename Param = AbstractServerSession*>
class RESTful
{
public:
    using Func = std::function<Return(Param param, ParsedUrl &&resources)>;
    RESTful(const std::string &urlPrefix) : m_urlPrefix(urlPrefix) {}

    /*!
     * \brief setMethodCallback
     *
     * Sets the method callback and parameters
     *
     * \param method to set, most common are GET, POST, PUT, PATCH and DELETE
     * \param parameters accepted by this urlPrefix
     */
    template<typename T>
    void setMethodCallback(const std::string &method, std::initializer_list<std::string> resources = {})
    {
        auto callback = [&](Getodac::AbstractServerSession *serverSession, Getodac::ParsedUrl &&resources) {
            return std::make_shared<T>(serverSession, std::move(resources));
        };
        m_methods.emplace(std::make_pair(method, std::make_pair(callback, resources)));
    }

    /*!
     * \brief setMethodCallback
     *
     * Sets the method callback and parameters
     *
     * \param method to set, most common are GET, POST, PUT, PATCH and DELETE
     * \param custom callback function
     * \param parameters accepted by this urlPrefix
     */
    void setMethodCallback(const std::string &method, const Func &callback, std::initializer_list<std::string> resources = {})
    {
        m_methods.emplace(std::make_pair(method, std::make_pair(callback, resources)));
    }

    /*!
     * \brief canHanldle
     *
     *  Checks if it can handle this restful request
     *
     * \param url the url
     * \param method HTTP verb
     * \return true if it can handle this request
     */
    inline bool canHanldle(const std::string &url, const std::string &method)
    {
        bool res = url.size() >= m_urlPrefix.size() &&
                std::memcmp(url.c_str(), m_urlPrefix.c_str(), m_urlPrefix.size()) == 0 &&
                m_methods.find(method) != m_methods.end();

        if (res && url.size() > m_urlPrefix.size() &&
                url[m_urlPrefix.size()] != '/') {
            return false;
        }
        return res;
    }

    /*!
     * \brief parse
     *
     * Parses the given url and executes the method callback set by setMethodCallback and it returns the T value of that method
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


        // Search for ?
        QueryStrings queryStrings;
        auto qpos = findInSubstr(url, m_urlPrefix.size(), url.size() - m_urlPrefix.size(), '?');
        if (qpos != std::string::npos) {
            // time to parse the queryStrings
            // the parser only supports key1=value1&key2=value2... format
            ++qpos;
            for (const auto &kvPair : split(url, '&', qpos, url.size() - qpos)) {
                auto kv = split(url, '=', kvPair.first, kvPair.second);
                switch (kv.size()) {
                case 1:
                    queryStrings.emplace_back(std::make_pair(unEscape(url.substr(kv[0].first, kv[0].second)), ""));
                    break;
                case 2:
                    queryStrings.emplace_back(std::make_pair(unEscape(url.substr(kv[0].first, kv[0].second)),
                                                                        unEscape(url.substr(kv[1].first, kv[1].second))));
                    break;
                default:
                    throw ResponseStatusError{400, "Invalid query strings"};
                }
            }
            --qpos;
        } else {
            qpos = url.size();
        }

        // Split resources
        Resources resourses;
        bool searchForValue =  false;
        for (const auto &resourceChunck : split(url, '/', m_urlPrefix.size(), qpos - m_urlPrefix.size())) {
            if (searchForValue) {
                resourses.back().second = std::move(unEscape(url.substr(resourceChunck.first, resourceChunck.second)));
            } else {
                std::string resourcesName = std::move(url.substr(resourceChunck.first, resourceChunck.second));

                // the resource name should not be escaped
                auto it = methodResources.find(resourcesName);
                if ( it == methodResources.end()) {
                    if (resourses.empty()) {
                        resourses.emplace_back(std::make_pair(std::string{}, resourcesName));
                        continue;
                    } else {
                        throw ResponseStatusError{400, "Unknown resource name \"" + resourcesName + "\""};
                    }
                }
                methodResources.erase(it);
                resourses.emplace_back(std::make_pair(resourcesName, std::string{}));
            }
            searchForValue = !searchForValue;
        }

        return methodPair.first(param, std::move(std::make_pair(resourses, queryStrings)));
    }

private:

    inline char toHex(char ch)
    {
        return isdigit(ch) ? ch - '0' : 10 + tolower(ch) - 'a';
    }

    inline std::string unEscape(const std::string &in)
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
