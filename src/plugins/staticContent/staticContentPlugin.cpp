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

#include <getodac/abstract_server_session.h>
#include <getodac/abstract_service_session.h>
#include <getodac/exceptions.h>
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
Getodac::LRUCache<std::string, FileMapPtr> s_filesCache(100);
std::vector<std::pair<std::string,std::string>> s_urls;

class StaticContent : public Getodac::AbstractServiceSession
{
public:
    StaticContent(Getodac::AbstractServerSession *serverSession, const std::string &root, const std::string &path)
        : Getodac::AbstractServiceSession(serverSession)
    {
        try {
            auto p = boost::filesystem::canonical(path, root);
            m_file = s_filesCache.getValue(p.string());
            if (!m_file) {
                m_file = std::make_shared<boost::iostreams::mapped_file_source>(p);
                s_filesCache.put(p.string(), m_file);
            }
        } catch (const boost::filesystem::filesystem_error &e) {
            throw Getodac::ResponseStatusError(404, e.what());
        } catch (...) {
            throw Getodac::ResponseStatusError(404, "Unhandled error");
        }
    }

    // ServiceSession interface
    void headerFieldValue(const std::string &, const std::string &) override {}
    void headersComplete() override {}
    void body(const char *, size_t) override {}
    void requestComplete() override
    {
        m_serverSession->responseStatus(200);
        m_serverSession->responseEndHeader(m_file->size());
    }

    void writeResponse(Getodac::AbstractServerSession::Yield &yield) override
    {
        m_serverSession->write(yield, m_file->data(), m_file->size());
        m_serverSession->responseComplete();
    }

private:
    FileMapPtr m_file;
};

} // namespace

PLUGIN_EXPORT std::shared_ptr<Getodac::AbstractServiceSession> createSession(Getodac::AbstractServerSession *serverSession, const std::string &url, const std::string &/*method*/)
{
    for (const auto &pair : s_urls) {
        if (boost::starts_with(url, pair.first))
            return std::make_shared<StaticContent>(serverSession, pair.second, url.c_str() + pair.first.size());
    }

    return std::shared_ptr<Getodac::AbstractServiceSession>();
}

PLUGIN_EXPORT bool initPlugin(const std::string &confDir)
{
    try {
        namespace pt = boost::property_tree;
        pt::ptree properties;
        pt::read_info(boost::filesystem::path(confDir).append("/staticFiles.conf").string(), properties);
        for (const auto &p : properties.get_child("paths"))
            s_urls.emplace_back(std::make_pair(p.first, p.second.get_value<std::string>()));
    } catch (...) {
        return false;
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
