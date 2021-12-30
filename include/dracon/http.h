/*
    Copyright (C) 2021, BogDan Vatra <bogdan@kde.org>

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

#include <dracon/stream.h>

namespace Dracon {

namespace {

// https://www.w3.org/Protocols/rfc2616/rfc2616-sec10.html
static const std::unordered_map<uint16_t, std::string_view> StatusCodes = {

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

inline std::string_view statusCodeString(uint16_t status)
{
    auto it = StatusCodes.find(status);
    if (it != StatusCodes.end())
        return it->second;
    return StatusCodes.at(500);
}
} // namespace

static constexpr auto ChunkedData = std::numeric_limits<size_t>::max();

using Fields = std::unordered_map<std::string, std::string>;

class Response: public Fields
{
public:
    Response(uint16_t statusCode = 500, std::string_view body = {}, Fields && fields = {})
        : Fields(std::move(fields))
        , m_statusCode(statusCode)
        , m_body(body)
        , m_contentLength(body.size())
    {}

    Response &setStatusCode(uint16_t status_code)
    {
        m_statusCode = status_code;
        return *this;
    }
    uint16_t statusCode() const {return m_statusCode;}

    Response &setContentLength(size_t length)
    {
        m_contentLength = length;
        m_body.clear();
        return *this;
    }
    size_t contentLength() const {return m_contentLength;}

    Response &setKeepAlive(std::chrono::seconds seconds)
    {
        m_keep_alive = seconds;
        return *this;
    }
    std::chrono::seconds keepAlive() const {return m_keep_alive;}

    Response &setBody(std::string body)
    {
        m_body = std::move(body);
        m_contentLength = m_body.size();
        return *this;
    }
    const std::string &body() const {return m_body;}

    std::string toString(std::chrono::seconds keepAliveOverride = std::chrono::seconds{-1}) const
    {
        std::ostringstream res;
        uint16_t statusCode = m_statusCode;
        if (!statusCode) {
            statusCode = 500;
        }
        res << "HTTP/1.1 " << statusCodeString(statusCode);
        for (const auto &kv : *this)
            res << kv.first << ": " << kv.second << CrlfString;

        if (keepAliveOverride.count() == -1)
            keepAliveOverride = m_keep_alive;
        if (m_contentLength == ChunkedData)
            res << "Transfer-Encoding: chunked\r\n";
        else
            res << "Content-Length: " << m_contentLength << CrlfString;
        if (keepAliveOverride.count() > 0) {
            res << "Keep-Alive: timeout=" << keepAliveOverride.count() << CrlfString;
            res << "Connection: keep-alive\r\n";
        } else {
            res << "Connection: close\r\n";
        }
        res << CrlfString;
        if (!m_body.empty())
            res << m_body;
        return res.str();
    }

private:
    uint16_t m_statusCode;
    std::string m_body;
    size_t m_contentLength = 0;
    std::chrono::seconds m_keep_alive{-1};
};

inline AbstractStream &operator << (AbstractStream &stream, const Response &res)
{
    using namespace std::chrono_literals;
    if (res.keepAlive().count() != -1)
        stream.setKeepAlive(res.keepAlive());
    if (res.contentLength() != ChunkedData)
        stream.setSessionTimeout(std::max(stream.sessionTimeout(),
                                        10s + std::chrono::seconds(res.contentLength() / (512 * 1024))));
    stream.write(res.toString(stream.keepAlive()));
    return stream;
}

class Request : public Fields
{
public:
    enum class State {
        Uninitialized,
        ProcessingUrl,
        ProcessingHeader,
        HeadersCompleted,
        ProcessingBody,
        Completed,
    };
    using BodyCallback = std::function<void(std::string_view)>;
public:
    Request() = default;

    State state() const noexcept { return m_state; }
    void setState(enum State s) noexcept { m_state = s; }

    const std::string &url() const noexcept { return m_url; }
    void setUrl(std::string &&url) noexcept { m_url = std::move(url); }

    const std::string &method() const noexcept { return m_method; }
    void setMethod(std::string &&method) noexcept { m_method = std::move(method); }

    bool keepAlive() const noexcept { return m_keep_alive; }
    void setKeepAlive(bool keep) noexcept { m_keep_alive = keep; }

    void appendBodyCallback(const BodyCallback &callback, size_t max_size = std::numeric_limits<size_t>::max() - 1) noexcept
    {
        m_maxBodySize = max_size;
        m_callback = callback;
    }

    void appendBody(std::string_view body) noexcept(false)
    {
        if (!m_callback)
            throw Response(400, std::string_view{"unexpected body"});
        m_callback(body);
    }

    size_t contentLength() const
    {
        auto it = find("Content-Length");
        if (it != end()) {
            char *end;
            auto data = it->second.data();
            auto len = std::strtoull(it->second.data(), &end, 10);
            if (end == data + it->second.size())
                return len;
        }
        return Dracon::ChunkedData;
    }

    size_t maxBodySize() const noexcept
    {
        return m_maxBodySize;
    }

private:
    bool m_keep_alive = false;
    std::string m_url;
    std::string m_method;
    BodyCallback m_callback;
    enum State m_state = State::Uninitialized;
    size_t m_maxBodySize = 0;
};

inline AbstractStream &operator >> (AbstractStream &stream, Request &req)
{
    auto it = req.find("Expect");
    if (it != req.end() && it->second == "100-continue") {
        size_t contentLength = req.contentLength();
        if (contentLength != ChunkedData && req.maxBodySize() < contentLength)
            throw 417; // Expectation Failed
        else
            stream << Response{100};
    }

    stream.read(req);
    return stream;
}

namespace Literals {
inline Response operator "" _http(unsigned long long int status)
{
    return Response{uint16_t(status)};
}
} // namespace Literals

} // namespace dracon
