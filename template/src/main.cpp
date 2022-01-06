/*
    Copyright (C) 2022 by BogDan Vatra <bogdan@kde.org>

    Permission to use, copy, modify, and/or distribute this software for any purpose with or without fee is hereby granted.

    THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

#include <filesystem>
#include <random>
#include <unordered_set>
#include <string>
#include <shared_mutex>


#include <dracon/http.h>
#include <dracon/restful.h>
#include <dracon/stream.h>
#include <dracon/logging.h>

#ifdef __READ_CONF__
#include <boost/algorithm/string.hpp>
#include <boost/property_tree/info_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#endif

#include <nlohmann/json.hpp>
using json = nlohmann::json;

TaggedLogger<> g_logger{"MyCoolProject"};

namespace {
Dracon::RESTfulRouterType s_restullV1RootNode("/v1/");
const std::string device_id{"device"};
std::vector<std::string> s_devices;
std::shared_mutex s_mutex; // getodac is a highly concurrent HTTP server,
                           // therefore all resources must be protected properly
}

void getDevices(const Dracon::ParsedRoute &route, Dracon::AbstractStream &stream, Dracon::Request &req)
{
    // The request at this point is partial, next line will read the rest of the request
    stream >> req;

    json res = json::array();
    // as this function is use by both device and device/{device} routes
    // we need to check if we have the {device} resource and return the
    // appropriate results
    {
        std::shared_lock lock{s_mutex};
        auto it = route.capturedResources.find(device_id);
        if (it != route.capturedResources.end()) {
            size_t idx = std::stoul(it->second.c_str());
            if (idx >= s_devices.size())
                throw 400; // bad request
            res.push_back({ {"id:", idx}, {"name", s_devices[idx]}});
        } else {
            for (size_t i = 0; i < s_devices.size(); ++i)
                res.push_back({ {"id:", i}, {"name", s_devices[i]}});
        }
    } // don't keep the mutex locked while we're sending the data
#ifndef __USE_CHUNCKED
    stream << Dracon::Response{200 /* res code */,
                               res.dump(), /* body */
                               {{"Content-Type","application/json"}} /* headers */
                              };
#else
    // the following code is doing the same as the above one
    // is here only to show how to do Chuncked data transfer
    // more examples, including processing the data in an worker
    // thread, you can find in GETodac's test plugin
    stream << Dracon::Response{200, {}, {{"Content-Type","application/json"}}}.setContentLength(Dracon::ChunkedData);
    Dracon::ChunkedStream chunkedStream{stream};
    Dracon::OStreamBuffer buff{chunkedStream};
    std::ostream str{&buff};
    str << res.dump();
#endif
}

void postDevices(const Dracon::ParsedRoute &, Dracon::AbstractStream &stream, Dracon::Request &req)
{
    // The request at this point is partial,
    // next lines will read the rest of the request including the body
    std::string body;
    req.appendBodyCallback([&](std::string_view buff){
        body.append(buff);
        if (body.size() > 512 * 1024)
            throw 400; // bad request
    }, 512 * 1024);
    stream >> req;

    auto jb = json::parse(body);
    {
        std::unique_lock lock{s_mutex};
        s_devices.clear();
        for (const auto & jv : jb)
            s_devices.push_back(jv);
    } // don't keep the mutex locked while we're sending the data

    stream << Dracon::Response{200};
}

void patchDevice(const Dracon::ParsedRoute &route, Dracon::AbstractStream &stream, Dracon::Request &req)
{
    // The request at this point is partial,
    // next lines will read the rest of the request including the body
    std::string body;
    req.appendBodyCallback([&](std::string_view buff){
        body.append(buff);
        if (body.size() > 512 * 1024)
            throw 400; // bad request
    }, 512 * 1024);
    stream >> req;

    auto it = route.capturedResources.find(device_id);
    if (it == route.capturedResources.end())
        throw 404; // not found

    {
        std::unique_lock lock{s_mutex};
        size_t idx = std::stoul(it->second.c_str());
        if (idx >= s_devices.size())
            throw 400; // bad request
        auto jb = json::parse(body);
        s_devices[idx] = jb["name"];
    } // don't keep the mutex locked while we're sending the data

    stream << Dracon::Response{200};
}

void deleteDevice(const Dracon::ParsedRoute &route, Dracon::AbstractStream &stream, Dracon::Request &req)
{
    // The request at this point is partial, next line will read the rest of the request
    stream >> req;

    auto it = route.capturedResources.find(device_id);
    if (it == route.capturedResources.end())
        throw 404; // not found

    size_t idx = std::stoul(it->second.c_str());
    {
        std::unique_lock lock{s_mutex};
        if (idx >= s_devices.size())
            throw 400; // bad request
        s_devices.erase(s_devices.begin() + idx);
    } // don't keep the mutex locked while we're sending the data
    stream << Dracon::Response{200};
}

#ifdef __READ_CONF__
std::string g_tokenAlgorithm;
std::string g_tokenSecret;
#endif

PLUGIN_EXPORT bool init_plugin(const std::string &confDir)
{
    try {
        INFO(g_logger) << "Initializing REST API plugin ...";
#ifdef __READ_CONF__
        const std::unordered_set<std::string> supportedAlgorithms{"HS256", "HS384", "HS512"};

        namespace pt = boost::property_tree;
        pt::ptree properties;
        const auto confPath = std::filesystem::path(confDir).append("MyCoolProject.conf");
        DEBUG(g_logger) << "Loading conf file from " << confPath;

        pt::read_info(confPath.string(), properties);
        g_tokenAlgorithm = properties.get<std::string>("signing.algorithm", "HS256");
        boost::to_upper(g_tokenAlgorithm);
        if (supportedAlgorithms.find(g_tokenAlgorithm) == supportedAlgorithms.end()) {
            FATAL(g_logger) << "Invalid algorithm " << g_tokenAlgorithm;
            throw std::runtime_error{"Invalid algorithm"};
        }
        g_tokenSecret = properties.get<std::string>("signing.secret");
        if (g_tokenSecret.empty()) {
            g_tokenSecret.resize(31);
            std::random_device rd;
            std::mt19937 gen(rd());
            std::generate(g_tokenSecret.begin(), g_tokenSecret.end(), gen);
        }
        // do something with properties.get<std::string>("postgresql.connection_string")
#endif

        // v1 routes, here you can create highly complicated routes.

        // devices
        s_restullV1RootNode.createRoute("devices")
                ->addMethodHandler("GET", Dracon::sessionHandler(getDevices))
                .addMethodHandler("POST", Dracon::sessionHandler(postDevices));

        // devices/{device}
        s_restullV1RootNode.createRoute("devices/{device}")
                ->addMethodHandler("GET", Dracon::sessionHandler(getDevices))
                .addMethodHandler("PATCH", Dracon::sessionHandler(patchDevice))
                .addMethodHandler("DELETE", Dracon::sessionHandler(deleteDevice));

    } catch (const std::exception &e) {
        FATAL(g_logger) << e.what();
        return false;
    } catch (...) {
        FATAL(g_logger) << "Unknown fatal error";
        return false;
    }
    INFO(g_logger) << " ... completed";
    return true;
}

PLUGIN_EXPORT uint32_t plugin_order()
{
    // The server calls this function to get the plugin order
    return 0;
}

PLUGIN_EXPORT Dracon::HttpSession create_session(Dracon::Request &req)
{
    const auto &url = req.url();
    const auto &method = req.method();
    return s_restullV1RootNode.createHandler(url, method);
}

PLUGIN_EXPORT void destory_plugin()
{
    // This function is called by the server when it closes. The plugin should wait in this function until it finishes the clean up.
}
