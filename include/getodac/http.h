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

#include <chrono>
#include <functional>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>

#include <getodac/stream.h>

namespace Getodac {

namespace {

// https://www.w3.org/Protocols/rfc2616/rfc2616-sec10.html
static const std::unordered_map<uint16_t, std::string_view> status_codes = {

    // Informational 1xx
    {100, "100 Continue\r\n"},
    {101, "101 Switching Protocols\r\n"},

    // Successful 2xx
    {200, "200 OK\r\n"},
    {201, "201 Created\r\n"},
    {202, "202 Accepted\r\n"},
    {203, "203 Non-Authoritative Information\r\n"},
    {204, "204 No Content\r\n"},
    {205, "205 Reset Content\r\n"},
    {206, "206 Partial Content\r\n"},

    // Redirection 3xx
    {300, "300 Multiple Choices\r\n"},
    {301, "301 Moved Permanently\r\n"},
    {302, "302 Found\r\n"},
    {303, "303 See Other\r\n"},
    {304, "304 Not Modified\r\n"},
    {305, "305 Use Proxy\r\n"},
    {306, "306 Switch Proxy\r\n"},
    {307, "307 Temporary Redirect\r\n"},

    // Client Error 4xx
    {400, "400 Bad Request\r\n"},
    {401, "401 Unauthorized\r\n"},
    {402, "402 Payment Required\r\n"},
    {403, "403 Forbidden\r\n"},
    {404, "404 Not Found\r\n"},
    {405, "405 Method Not Allowed\r\n"},
    {406, "406 Not Acceptable\r\n"},
    {407, "407 Proxy Authentication Required\r\n"},
    {408, "408 Request Timeout\r\n"},
    {409, "409 Conflict\r\n"},
    {410, "410 Gone\r\n"},
    {411, "411 Length Required\r\n"},
    {412, "412 Precondition Failed\r\n"},
    {413, "413 Request Entity Too Large\r\n"},
    {414, "414 Request-URI Too Long\r\n"},
    {415, "415 Unsupported Media Type\r\n"},
    {416, "415 Requested Range Not Satisfiable\r\n"},
    {417, "417 Expectation Failed\r\n"},

    // Server Error 5xx
    {500, "500 Internal Server Error\r\n"},
    {501, "501 Not Implemented\r\n"},
    {502, "502 Bad Gateway\r\n"},
    {503, "503 Service Unavailable\r\n"},
    {504, "504 Gateway Timeout\r\n"},
    {505, "505 HTTP Version Not Supported\r\n"}
};

inline std::string_view status_code_string(uint16_t status)
{
    auto it = status_codes.find(status);
    if (it != status_codes.end())
        return it->second;
    return status_codes.at(500);
}
} // namespace

enum {
    Chunked_Data = std::numeric_limits<size_t>::max()
};

using fields = std::unordered_map<std::string, std::string>;

class response: public fields
{
public:
    response(uint16_t status_code = 500, std::string_view body = {}, fields && fields_ = {})
        : fields(std::move(fields_))
        , m_status_code(status_code)
        , m_body(body)
        , m_content_length(body.size())
    {}

    response &status_code(uint16_t status_code)
    {
        m_status_code = status_code;
        return *this;
    }
    uint16_t status_code() const {return m_status_code;}

    response &content_length(size_t length)
    {
        m_content_length = length;
        m_body.clear();
        return *this;
    }
    uint16_t content_length() const {return m_content_length;}

    response &keep_alive(std::chrono::seconds seconds)
    {
        m_keep_alive = seconds;
        return *this;
    }
    std::chrono::seconds keep_alive() const {return m_keep_alive;}

    response &body(std::string body)
    {
        m_body = std::move(body);
        m_content_length = m_body.size();
        return *this;
    }
    const std::string &body() const {return m_body;}

    std::string to_string(std::chrono::seconds keep_alive_override = std::chrono::seconds{-1}) const
    {
        std::ostringstream res;
        uint16_t status_code = m_status_code;
        if (!status_code) {
            status_code = 500;
        }
        res << "HTTP/1.1 " << status_code_string(status_code);
        for (const auto &kv : *this)
            res << kv.first << ": " << kv.second << crlf_string;

        if (keep_alive_override.count() == -1)
            keep_alive_override = m_keep_alive;
        if (m_content_length == Chunked_Data)
            res << "Transfer-Encoding: chunked\r\n";
        else
            res << "Content-Length: " << m_content_length << crlf_string;
        if (keep_alive_override.count() > 0) {
            res << "Keep-Alive: timeout=" << keep_alive_override.count() << crlf_string;
            res << "Connection: keep-alive\r\n";
        } else {
            res << "Connection: close\r\n";
        }
        res << crlf_string;
        if (!m_body.empty())
            res << m_body;
        return res.str();
    }

private:
    uint16_t m_status_code;
    std::string m_body;
    size_t m_content_length = 0;
    std::chrono::seconds m_keep_alive{-1};
};

inline abstract_stream &operator << (abstract_stream &stream, const response &res)
{
    if (res.keep_alive().count() != -1)
        stream.keep_alive(res.keep_alive());
    stream.write(res.to_string(stream.keep_alive()));
    return stream;
}

class request : public fields
{
public:
    enum class state {
        uninitialized,
        processing_url,
        processing_header,
        headers_completed,
        processing_body,
        completed,
    };
    using BodyCallback = std::function<void(std::string_view)>;
public:
    request() = default;

    state state() const noexcept { return m_state; }
    void state(enum state s) noexcept { m_state = s; }

    const std::string &url() const noexcept { return m_url; }
    void url(std::string &&url) noexcept { m_url = std::move(url); }

    const std::string &method() const noexcept { return m_method; }
    void method(std::string &&method) noexcept { m_method = std::move(method); }

    bool keep_alive() const noexcept { return m_keep_alive; }
    void keep_alive(bool keep) noexcept { m_keep_alive = keep; }

    void append_body_callback(const BodyCallback &callback) noexcept { m_callback = callback; }
    void append_body(std::string_view body) noexcept(false)
    {
        if (!m_callback)
            throw response(400, std::string_view{"unexpected body"});
        m_callback(body);
    }

private:
    bool m_keep_alive = false;
    std::string m_url;
    std::string m_method;
    BodyCallback m_callback;
    enum state m_state = state::uninitialized;
};

inline abstract_stream &operator >> (abstract_stream &stream, request &req)
{
    stream.read(req);
    return stream;
}

} // namespace Getodac
