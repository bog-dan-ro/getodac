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
#include <dracon/http.h>
#include <dracon/logging.h>
#include <dracon/plugin.h>
#include <dracon/utils.h>

#include <filesystem>

#include <boost/algorithm/string.hpp>
#include <boost/iostreams/device/mapped_file.hpp>
#include <boost/property_tree/info_parser.hpp>
#include <boost/property_tree/ptree.hpp>

namespace {
class FileMap : public boost::iostreams::mapped_file_source
{
public:
    FileMap(const std::filesystem::path &path, std::filesystem::file_time_type lastWriteTime)
        : boost::iostreams::mapped_file_source(path)
        , m_lastWriteTime(lastWriteTime)
    {}
    ~FileMap() { close(); }
    inline std::filesystem::file_time_type lastWriteTime() const { return m_lastWriteTime; }
private:
    std::filesystem::file_time_type m_lastWriteTime;
};
using FileMapPtr = std::shared_ptr<FileMap>;
dracon::lru_cache<std::string, FileMapPtr> s_filesCache{100};

std::mutex s_filesCacheMutex;
std::string s_default_file;
std::vector<std::pair<std::string, std::string>> s_urls;
bool s_allow_symlinks = false;
TaggedLogger<> logger{"staticContent"};
std::unique_ptr<dracon::simple_timer> g_timer;

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

void static_content_session(const std::filesystem::path &root, const std::filesystem::path &path, dracon::abstract_stream& stream, dracon::request& req)
{
    stream >> req;
    auto p = (root / path).lexically_normal();
    if (!s_allow_symlinks)
        p = std::filesystem::canonical(p);
    if (!boost::starts_with(p, root)) { // make sure we don't server files outside the root
        WARNING(logger) << "path \"" << p << "\" is outside the root \"" << root << "\"";
        throw dracon::response{400};
    }
    if (std::filesystem::is_directory(p))
        p /= s_default_file;
    TRACE(logger) << "Serving " << p.string();
    std::unique_lock<std::mutex> lock{s_filesCacheMutex};
    auto file = s_filesCache.value(p.string());
    auto lastWriteTime = std::filesystem::last_write_time(p);
    if (!file || file->lastWriteTime() != lastWriteTime) {
        file = std::make_shared<FileMap>(p, lastWriteTime);
        s_filesCache.put(p.string(), file);
    }

    {
        dracon::response res{200};
        res["Content-Type"] = mimeType(p.extension().string());
        res.content_length(file->size());
        stream << res;
    }
    stream.write({file->data(), file->size()});
}

} // namespace

PLUGIN_EXPORT dracon::HttpSession create_session(const dracon::request &req) {
    if (req.method() != "GET")
        return {};
    auto &url = req.url();
    for (const auto &pair : s_urls) {
        if (boost::starts_with(url, pair.first)) {
            if (boost::starts_with(pair.first, "/~")) {
                auto pos = url.find('/', 1);
                if (pos == std::string::npos)
                    pos = url.size();
                std::filesystem::path root_path{pair.second};
                auto user = dracon::unescape_url(url.substr(2, pos - 2));
                if (user == "." || user == "..")
                    break; // avoid GET /~../../etc/passwd HTTP/1.0 requests

                root_path /= user;
                root_path /= "public_html";
                std::filesystem::path file_path;
                if (url.size() - pos > 1)
                    file_path /= dracon::unescape_url(url.substr(pos + 1, url.size() - pos - 1));
                return std::bind<void>(static_content_session,
                                       root_path,
                                       file_path.lexically_normal(),
                                       std::placeholders::_1,
                                       std::placeholders::_2);
            } else {
                return std::bind<void>(static_content_session,
                                       std::filesystem::path{pair.second},
                                       std::filesystem::path{dracon::unescape_url(url.c_str() + pair.first.size())}.lexically_normal(),
                                       std::placeholders::_1,
                                       std::placeholders::_2);
            }
        }
    }
    return {};
}

PLUGIN_EXPORT bool init_plugin(const std::string &confDir)
{
    using namespace std::chrono_literals;
    INFO(logger) << "Initializing plugin";
    namespace pt = boost::property_tree;
    pt::ptree properties;
    pt::read_info(std::filesystem::path(confDir).append("staticFiles.conf").string(), properties);
    for (const auto &p : properties.get_child("paths")) {
        DEBUG(logger) << "Mapping \"" << p.first << "\" to \"" << p.second.get_value<std::string>() << "\"";
        s_urls.emplace_back(std::make_pair(p.first, p.second.get_value<std::string>()));
    }
    s_default_file = properties.get("default_file", "");
    s_allow_symlinks = properties.get("allow_symlinks", false);
    g_timer = std::make_unique<dracon::simple_timer>([]{
        std::unique_lock<std::mutex> lock(s_filesCacheMutex);
        for (auto it = s_filesCache.begin(); it != s_filesCache.end();) {
            if (it->second.use_count() == 1)
                it = s_filesCache.erase(it);
            else
                ++it;
        }
    }, 60s);
    return !s_urls.empty();
}

PLUGIN_EXPORT uint32_t plugin_order()
{
    return UINT32_MAX;
}

PLUGIN_EXPORT void destory_plugin()
{
    g_timer.reset();
}
