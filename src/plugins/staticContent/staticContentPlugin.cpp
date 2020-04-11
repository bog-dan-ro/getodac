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
*/

#include <getodac/abstract_server_session.h>
#include <getodac/abstract_service_session.h>
#include <getodac/exceptions.h>
#include <getodac/logging.h>
#include <getodac/restful.h>
#include <getodac/utils.h>

#include <iostream>
#include <mutex>

#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <boost/iostreams/device/mapped_file.hpp>
#include <boost/property_tree/info_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/utility/string_view.hpp>

namespace {
using FileMapPtr = std::shared_ptr<boost::iostreams::mapped_file_source>;
Getodac::LRUCache<std::string, std::pair<std::time_t, FileMapPtr>> s_filesCache(100);
std::string s_default_file;
std::vector<std::pair<std::string, std::string>> s_urls;
bool s_allow_symlinks = false;

TaggedLogger<> logger{"staticContent"};

inline std::string mimeType(boost::string_view ext)
{
    if (ext == ".htm")  return "text/html";
    if (ext == ".html") return "text/html";
    if (ext == ".php")  return "text/html";
    if (ext == ".css")  return "text/css";
    if (ext == ".js")   return "application/javascript";
    if (ext == ".json") return "application/json";
    if (ext == ".xml")  return "application/xml";
    if (ext == ".png")  return "image/png";
    if (ext == ".jpe")  return "image/jpeg";
    if (ext == ".jpeg") return "image/jpeg";
    if (ext == ".jpg")  return "image/jpeg";
    if (ext == ".gif")  return "image/gif";
    if (ext == ".bmp")  return "image/bmp";
    if (ext == ".tiff") return "image/tiff";
    if (ext == ".tif")  return "image/tiff";
    if (ext == ".svg")  return "image/svg+xml";
    if (ext == ".svgz") return "image/svg+xml";
    if (ext == ".txt")  return "text/plain";
    if (ext == ".webp")  return "image/webp";
    if (ext == ".webm")  return "video/webmx";
    if (ext == ".weba")  return "audio/webm";
    if (ext == ".swf")  return "application/x-shockwave-flash";
    if (ext == ".flv")  return "video/x-flv";
    return "application/octet-stream";
}

class StaticContent : public Getodac::AbstractServiceSession
{
public:
    StaticContent(Getodac::AbstractServerSession *serverSession, const boost::filesystem::path &root, const boost::filesystem::path &path)
        : Getodac::AbstractServiceSession(serverSession)
    {
        try {
            auto p = (root / path).lexically_normal();
            if (!s_allow_symlinks)
                p = boost::filesystem::canonical(p);
            if (!boost::starts_with(p, root)) // make sure we don't server files outside the root
                throw std::runtime_error{"File not found"};
            if (boost::filesystem::is_directory(p))
                p /= s_default_file;

            TRACE(logger) << "Serving " << p.string();
            m_file = s_filesCache.getValue(p.string());
            auto lastWriteTime = boost::filesystem::last_write_time(p);
            if (!m_file.second || m_file.first != lastWriteTime) {
                m_file = std::make_pair(lastWriteTime, std::make_shared<boost::iostreams::mapped_file_source>(p));
                s_filesCache.put(p.string(), m_file);
            }
            m_mimeType = mimeType(p.extension().string());
        } catch (const std::exception &e) {
            TRACE(logger) << e.what();
            throw Getodac::ResponseStatusError(404, e.what());
        } catch (...) {
            throw Getodac::ResponseStatusError(404, "Unhandled error");
        }
    }

    // ServiceSession interface
    void headerFieldValue(const std::string &, const std::string &) override {}
    bool acceptContentLength(size_t) override {return false;}
    void headersComplete() override {}
    void body(const char *, size_t) override {}
    void requestComplete() override
    {
        m_serverSession->responseStatus(200);
        m_serverSession->responseHeader("Content-Type", m_mimeType);
        m_serverSession->responseEndHeader(m_file.second->size());
    }

    void writeResponse(Getodac::AbstractServerSession::Yield &yield) override
    {
        m_serverSession->write(yield, m_file.second->data(), m_file.second->size());
        m_serverSession->responseComplete();
    }

private:
    std::pair<std::time_t, FileMapPtr> m_file;
    std::string m_mimeType;
};

} // namespace

PLUGIN_EXPORT std::shared_ptr<Getodac::AbstractServiceSession> createSession(Getodac::AbstractServerSession *serverSession, const std::string &url, const std::string &/*method*/)
{
    for (const auto &pair : s_urls) {
        if (boost::starts_with(url, pair.first)) {
            if (boost::starts_with(pair.first, "/~")) {
                auto pos = url.find('/', 1);
                if (pos == std::string::npos)
                    pos = url.size();
                boost::filesystem::path root_path{pair.second};
                auto user = url.substr(2, pos - 2);
                if (user == "." || user == "..")
                    break; // avoid GET /~../../etc/passwd HTTP/1.0 requests

                root_path /= user;
                root_path /= "public_html";
                boost::filesystem::path file_path;
                if (url.size() - pos > 1)
                    file_path /= url.substr(pos + 1, url.size() - pos - 1);
                return std::make_shared<StaticContent>(serverSession, root_path.string(), file_path.lexically_normal());
            } else {
                return std::make_shared<StaticContent>(serverSession, pair.second, boost::filesystem::path{url.c_str() + pair.first.size()}.lexically_normal());
            }
        }
    }

    return std::shared_ptr<Getodac::AbstractServiceSession>();
}

PLUGIN_EXPORT bool initPlugin(const std::string &confDir)
{
    INFO(logger) << "Initializing plugin";
    namespace pt = boost::property_tree;
    pt::ptree properties;
    pt::read_info(boost::filesystem::path(confDir).append("/staticFiles.conf").string(), properties);
    for (const auto &p : properties.get_child("paths")) {
        DEBUG(logger) << "Mapping \"" << p.first << "\" to \"" << p.second.get_value<std::string>() << "\"";
        s_urls.emplace_back(std::make_pair(p.first, p.second.get_value<std::string>()));
    }
    s_default_file = properties.get("default_file", "");
    s_allow_symlinks = properties.get("allow_symlinks", false);

    return !s_urls.empty();
}

PLUGIN_EXPORT uint32_t pluginOrder()
{
    return UINT32_MAX;
}

PLUGIN_EXPORT void destoryPlugin()
{
}
