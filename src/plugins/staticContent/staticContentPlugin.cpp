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

namespace {
using FileMapPtr = std::shared_ptr<boost::iostreams::mapped_file_source>;
Getodac::LRUCache<std::string, std::pair<std::time_t, FileMapPtr>> s_filesCache(100);
std::vector<std::pair<std::string, std::string>> s_urls;
TaggedLogger<> logger{"staticContent"};

class StaticContent : public Getodac::AbstractServiceSession
{
public:
    StaticContent(Getodac::AbstractServerSession *serverSession, const std::string &root, const std::string &path)
        : Getodac::AbstractServiceSession(serverSession)
    {
        try {
            auto p = boost::filesystem::canonical(path, root);
            TRACE(logger) << "Serving " << p.string();
            m_file = s_filesCache.getValue(p.string());
            auto lastWriteTime = boost::filesystem::last_write_time(p);
            if (!m_file.second || m_file.first != lastWriteTime) {
                m_file = std::make_pair(lastWriteTime, std::make_shared<boost::iostreams::mapped_file_source>(p));
                s_filesCache.put(p.string(), m_file);
            }
        } catch (const boost::filesystem::filesystem_error &e) {
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
        m_serverSession->responseEndHeader(m_file.second->size());
    }

    void writeResponse(Getodac::AbstractServerSession::Yield &yield) override
    {
        m_serverSession->write(yield, m_file.second->data(), m_file.second->size());
        m_serverSession->responseComplete();
    }

private:
    std::pair<std::time_t, FileMapPtr> m_file;
};

} // namespace

PLUGIN_EXPORT std::shared_ptr<Getodac::AbstractServiceSession> createSession(Getodac::AbstractServerSession *serverSession, const std::string &url, const std::string &/*method*/)
{
    for (const auto &pair : s_urls) {
        if (boost::starts_with(url, pair.first)) {
            if (boost::starts_with(pair.first, "/~")) {
                auto pos = url.find('/', 1);
                if (pos == std::string::npos)
                    break;
                return std::make_shared<StaticContent>(serverSession, pair.second, url.substr(2, pos - 1) + "public_html" + url.substr(pos, url.size() - pos));
            } else {
                return std::make_shared<StaticContent>(serverSession, pair.second, url.c_str() + pair.first.size());
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

    return !s_urls.empty();
}

PLUGIN_EXPORT uint32_t pluginOrder()
{
    return UINT32_MAX;
}

PLUGIN_EXPORT void destoryPlugin()
{
}
