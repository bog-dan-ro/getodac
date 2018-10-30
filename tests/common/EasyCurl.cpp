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
*/

#include "EasyCurl.h"

#include <cstring>
#include <boost/algorithm/string.hpp>
#include <vector>

namespace Getodac {
namespace Test {

EasyCurl::EasyCurl()
{
    m_curl = curl_easy_init();
    if (!m_curl)
        throw std::runtime_error{"Can't init CUrl"};
    setOpt(CURLOPT_WRITEFUNCTION, &write_callback);
    setOpt(CURLOPT_HEADERFUNCTION, &header_callback);
}

EasyCurl::~EasyCurl()
{
    curl_slist_free_all(m_headersList);
    curl_easy_cleanup(m_curl);
}

EasyCurl &EasyCurl::setUrl(const std::string &url)
{
    setOpt(CURLOPT_URL, url.c_str());
    return *this;
}

EasyCurl &EasyCurl::setHeaders(const EasyCurl::Headers &headers)
{
    if (m_headersList) {
        curl_slist_free_all(m_headersList);
        m_headersList = nullptr;
    }

    for (const auto &kv: headers)
        m_headersList = curl_slist_append(m_headersList, (kv.first + ": " + kv.second).c_str());

    setOpt(CURLOPT_HTTPHEADER, m_headersList);
    return *this;
}

EasyCurl::Response EasyCurl::request(const std::string &method, std::string upload) const
{
    Response res;
    setOpt(CURLOPT_WRITEDATA, &res);
    setOpt(CURLOPT_HEADERDATA, &res);
    setOpt(CURLOPT_CUSTOMREQUEST, method.c_str());

    if (upload.size()) {
        setOpt(CURLOPT_READDATA, &upload);
        setOpt(CURLOPT_READFUNCTION, &read_callback);
        setOpt(CURLOPT_UPLOAD, 1L);
        setOpt(CURLOPT_INFILESIZE_LARGE, curl_off_t(upload.size()));
    }

    auto err = curl_easy_perform(m_curl);
    if (err != CURLE_OK)
        throw std::runtime_error{curl_easy_strerror(err)};

    return res;
}

size_t EasyCurl::read_callback(char *buffer, size_t size, size_t nitems, std::string *upload)
{
    size_t sz = std::min(size * nitems, upload->size());
    mempcpy(buffer, upload->c_str(), sz);
    upload->erase(0, sz);
    return sz;
}

size_t EasyCurl::write_callback(char *ptr, size_t size, size_t nmemb, EasyCurl::Response *response)
{
    response->body.append(ptr, size * nmemb);
    return size * nmemb;
}

size_t EasyCurl::header_callback(char *buffer, size_t size, size_t nitems, EasyCurl::Response *response)
{
    if (size * nitems <= 2)
        return size * nitems;

    std::string header{buffer, size * nitems - 2};
    if (response->status.empty()) {
        std::vector<std::string> status;
        boost::split(status, header, boost::is_any_of(" "));
        response->status = status.size() > 1 ? status[1] : "unknown";
    } else {
        std::vector<std::string> kv;
        boost::split(kv, header, boost::is_any_of(":"));
        std::string key = kv.empty() ? header : kv[0];
        std::string value = kv.size() == 2 ? kv[1].substr(1) : std::string{};
        response->headers.emplace(key, value);
    }
    return size * nitems;
}

} // namespace Test
} // namespace Getodac
