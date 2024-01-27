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
*/

#include "EasyCurl.h"

#include <iostream>
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
    setOptions(CURLOPT_WRITEFUNCTION, &write_callback);
    setOptions(CURLOPT_HEADERFUNCTION, &header_callback);
    setOptions(CURLOPT_PATH_AS_IS, 1L);
}

EasyCurl::~EasyCurl()
{
    curl_slist_free_all(m_headersList);
    curl_easy_cleanup(m_curl);
}

EasyCurl &EasyCurl::setUrl(const std::string &url)
{
    setOptions(CURLOPT_URL, url.c_str());
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

    setOptions(CURLOPT_HTTPHEADER, m_headersList);
    return *this;
}

EasyCurl::Response EasyCurl::request(const std::string &method, std::string upload) const
{
    Response res;
    setOptions(CURLOPT_WRITEDATA, &res);
    setOptions(CURLOPT_HEADERDATA, &res);
    setOptions(CURLOPT_CUSTOMREQUEST, method.c_str());

    if (upload.size()) {
        setOptions(CURLOPT_READDATA, &upload);
        setOptions(CURLOPT_READFUNCTION, &read_callback);
        setOptions(CURLOPT_UPLOAD, 1L);
        setOptions(CURLOPT_INFILESIZE_LARGE, curl_off_t(upload.size()));
    } else {
        setOptions(CURLOPT_READDATA, nullptr);
        setOptions(CURLOPT_READFUNCTION, nullptr);
        setOptions(CURLOPT_UPLOAD, 0L);
        setOptions(CURLOPT_INFILESIZE_LARGE, 0);
    }

    auto err = curl_easy_perform(m_curl);
    if (err != CURLE_OK) {
        std::cerr << curl_easy_strerror(err);
        throw std::runtime_error{curl_easy_strerror(err)};
    }

    return res;
}

std::string EasyCurl::escape(std::string_view str)
{
    auto allocatedStr = curl_escape(str.data(), str.length());
    std::string ss{allocatedStr};
    curl_free(allocatedStr);
    return ss;
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
    if (header.substr(0, 8) == "HTTP/1.1") {
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
