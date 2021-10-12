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
*/

#include <dracon/http.h>
#include <dracon/plugin.h>
#include <dracon/restful.h>
#include <dracon/logging.h>
#include <dracon/thread_worker.h>

#include <cassert>
#include <iostream>
#include <mutex>

using namespace std::chrono_literals;

namespace {

const std::string test100response{"100XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX"};
std::string test50mresponse;

Dracon::RESTfulRouterType s_testRootRestful("/test/rest/v1/");
Dracon::ThreadWorker s_threadWorker{10};

TaggedLogger<> logger{"test"};

void TestRESTGET(const Dracon::ParsedRoute& parsed_route, Dracon::AbstractStream& stream, Dracon::Request& req){
    stream >> req;
    stream << Dracon::Response{200, {}, {{"Content-Type","text/plain"}}}.contentLength(Dracon::ChunkedData);
    Dracon::ChunkedStream chunkedStream{stream};
    Dracon::OStreamBuffer buff{chunkedStream};
    std::ostream str{&buff};
    str << "Got " << parsed_route.capturedResources.size() << " captured resources\n";
    str << "and " << parsed_route.queryStrings.size() << " queries\n";
    str << "All methods but OPTIONS " << parsed_route.allButOPTIONSNodeMethods << " \n";
    for (const auto &resource : parsed_route.capturedResources) {
        str << "Resource name: " << resource.first << "  value: " << resource.second << std::endl;
    }
    for (const auto &query : parsed_route.queryStrings) {
        str << "Query name: " << query.first << "  value: " << query.second << std::endl;
    }
}

} // namespace

PLUGIN_EXPORT Dracon::HttpSession create_session(const Dracon::Request &req)
{
    using namespace Dracon::Literals;
    auto &url = req.url();
    if (url == "/test0")
        return [&](Dracon::AbstractStream& stream, Dracon::Request& req){
            stream >> req;
            stream << 200_http;
        };

    if (url == "/test100")
        return [&](Dracon::AbstractStream& stream, Dracon::Request& req){
            stream >> req;
            stream << Dracon::Response{200, test100response};
        };

    if (url == "/test100Chunked")
        return [&](Dracon::AbstractStream& stream, Dracon::Request& req){
            stream >> req;
            stream << Dracon::Response{200}.contentLength(Dracon::ChunkedData);
            Dracon::ChunkedStream{stream}.write(test100response);
        };

    if (url == "/test50m")
        return [&](Dracon::AbstractStream& stream, Dracon::Request& req){
            stream >> req;
            stream << Dracon::Response{200}.contentLength(test50mresponse.size())
                   << test50mresponse;
        };

    if (url == "/test50mChunked")
        return [&](Dracon::AbstractStream& stream, Dracon::Request& req){
            stream >> req;
            stream << Dracon::Response{200}.contentLength(Dracon::ChunkedData);
            Dracon::ChunkedStream chuncked_stream{stream};
            uint32_t pos = 0;
            do {
                uint32_t chunkSize = 1 + rand() % (1024 * 1024);
                chunkSize = std::min<uint32_t>(chunkSize, test50mresponse.size() - pos);
                chuncked_stream.write({test50mresponse.c_str() + pos, chunkSize});
                pos += chunkSize;
            } while (pos < test50mresponse.size());
        };

    if (url == "/testWorker")
        return [&](Dracon::AbstractStream& stream, Dracon::Request& req){
            stream >> req;
            stream << Dracon::Response{200}.contentLength(Dracon::ChunkedData);
            auto wakeupper = stream.wakeupper();
            auto wait = std::make_shared<std::atomic_bool>();
            auto buffer = std::make_shared<std::string>();
            uint32_t size = 0;
            Dracon::ChunkedStream chuncked_stream{stream};
            do {
                wait->store(true);
                s_threadWorker.insertTask([=]{
                    // simulate some heavy work
                    std::this_thread::sleep_for(15ms);
                    uint32_t chunkSize = 1000 + (rand() % 4) * 1000;
                    buffer->resize(chunkSize);
                    for (uint32_t i = 0; i < chunkSize; ++i)
                        (*buffer)[i] = '0' + i % 10;
                    wait->store(false);
                    wakeupper->wakeUp();
                });
                do {
                    if (auto ec = stream.yield())
                        throw ec;
                } while (wait->load());
                chuncked_stream << *buffer;
                size += buffer->size();
            } while (size < 100000);
        };

    if (url == "/test50ms")
        return [&](Dracon::AbstractStream& stream, Dracon::Request& req){
            stream >> req;
            std::vector<Dracon::ConstBuffer> vec;
            vec.resize(51);
            for (int i = 1; i < 51; ++i) {
                vec[i].ptr = (void*)(test50mresponse.c_str() + 1024 * 1024 * (i - 1));
                vec[i].length = 1024 * 1024;
            }
            auto res = Dracon::Response{200}.contentLength(test50mresponse.size()).toString(stream.keepAlive());
            vec[0].ptr = res.data();
            vec[0].length = res.length();
            stream.write(vec);
        };

    if (url == "/echoTest")
        return [&](Dracon::AbstractStream& stream, Dracon::Request& req){
            stream.sessionTimeout(10s);
            std::string body;
            req.appendBodyCallback([&](std::string_view buff){
                body.append(buff);
            });
            stream >> req;
            if (std::strtoull(req["Content-Length"].data(), nullptr, 10) != body.size()) {
                throw 400;
            }
            stream << Dracon::Response{200}.contentLength(Dracon::ChunkedData);
            Dracon::ChunkedStream chunked_stream{stream};
            Dracon::OStreamBuffer buff{chunked_stream};
            std::ostream res{&buff};
            res << "~~~~ ContentLength: " << req["Content-Length"] << std::endl;
            res << "~~~~ Headers:\n";
            for (const auto &kv : req)
                res << kv.first << " : " << kv.second << std::endl;
            res << "~~~~ Body:\n" << body;
        };

    if (url == "/secureOnly")
        return [&](Dracon::AbstractStream& stream, Dracon::Request& req){
            if (!stream.isSecuredConnection())
                throw Dracon::Response{403, "Only secured connections allowed", {{"ErrorKey1","Value1"}, {"ErrorKey2","Value2"}}};
            stream >> req;
            stream << 200_http;
        };

    if (url == "/testExpectation")
        return [&](Dracon::AbstractStream& stream, Dracon::Request& req){
            if (req["Expect"] == "100-continue") {
                if (req["X-Continue"] != "100") {
                    throw Dracon::Response{417};
                }
            }
            req.appendBodyCallback([](std::string_view buff){
                (void)buff;
            });
            stream >> req;
            stream << 200_http;
        };

    if (url == "/testThowFromRequestComplete")
        return [&](Dracon::AbstractStream&, Dracon::Request&){
            throw 412;
        };

    if (url == "/testThowFromBody")
        return [&](Dracon::AbstractStream& stream, Dracon::Request& req){
            req.appendBodyCallback([](std::string_view){
                throw Dracon::Response{400, "Body too big, lose some weight",
                                        {{"BodyKey1", "Value1"},
                        {"BodyKey2", "Value2"}}};
            });
            stream >> req;
            stream << 200_http;
        };

    if (url == "/testThowFromWriteResponse")
        return [&](Dracon::AbstractStream& stream, Dracon::Request& req){
            stream >> req;
            throw Dracon::Response{409, "Throw from WriteResponse", {{"WriteRes1","Value1"}, {"WriteRes2","Value2"}}};
        };

    if (url == "/testThowFromWriteResponseStd")
        return [&](Dracon::AbstractStream& stream, Dracon::Request& req){
            stream >> req;
            throw std::runtime_error{"Throw from WriteResponseStd"};
        };

    if (url == "/testThowFromWriteResponseAfterWrite")
        return [&](Dracon::AbstractStream& stream, Dracon::Request& req){
            stream >> req;
            stream << Dracon::Response{200}.contentLength(Dracon::ChunkedData);
            throw std::runtime_error{"Unexpected error"};
        };

    if (url == "/testThrowAfterWakeup")
        return [&](Dracon::AbstractStream& stream, Dracon::Request& req){
            stream >> req;

            auto wakeupper = stream.wakeupper();
            auto wait = std::make_shared<std::atomic_bool>();
            wait->store(true);
            s_threadWorker.insertTask([=]{
                // simulate some heavy work
                std::this_thread::sleep_for(100ms);
                wait->store(false);
                wakeupper->wakeUp();
            });
            do {
                if (auto ec = stream.yield())
                    throw ec;
            } while (wait->load());
            throw 404;
        };

    // PPP stands for post, put, patch
    if (url == "/testPPP")
        return [&](Dracon::AbstractStream& stream, Dracon::Request& req){
            std::string body;
            req.appendBodyCallback([&](std::string_view buff){
                body.append(buff);
            });
            stream >> req;
            if (std::strtoull(req["Content-Length"].data(), nullptr, 10) != test50mresponse.size())
                throw Dracon::Response{400, "Invaid body size"};
            if (body != test50mresponse)
                throw Dracon::Response{400, "Invaid body"};
            stream << Dracon::Response{200}.contentLength(body.length()) << body;
        };

    return s_testRootRestful.createHandler(url, req.method());
}

PLUGIN_EXPORT bool init_plugin(const std::string &/*confDir*/)
{
    for (int i = 0; i < 50 * 1024 * 1024; ++i)
        test50mresponse += char(33 + (i % 93));
    s_testRootRestful.createRoute("customers")
            ->addMethodHandler("GET", Dracon::sessionHandler(TestRESTGET));
    s_testRootRestful.createRoute("customers/{customerId}")
            ->addMethodHandler("GET", Dracon::sessionHandler(TestRESTGET));
    s_testRootRestful.createRoute("customers/{customerId}/licenses")
            ->addMethodHandler("GET", Dracon::sessionHandler(TestRESTGET));
    s_testRootRestful.createRoute("customers/{customerId}/licenses/{licenseId}")
            ->addMethodHandler("GET", Dracon::sessionHandler(TestRESTGET));
    return true;
}

PLUGIN_EXPORT uint32_t plugin_order()
{
    return 9999999;
}

PLUGIN_EXPORT void destory_plugin()
{}
