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

#include <getodac/abstract_restful_session.h>
#include <getodac/abstract_server_session.h>
#include <getodac/abstract_service_session.h>


#include <cassert>
#include <iostream>
#include <mutex>

namespace {

const std::string test100response{"100XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX"};
std::string test50mresponse;

Getodac::RESTfulRouterType s_testRootRestful("/test/rest/v1/");

class BaseTestSession : public Getodac::AbstractServiceSession
{
public:
    explicit BaseTestSession(Getodac::AbstractServerSession *serverSession)
        : Getodac::AbstractServiceSession(serverSession)
    {}
    // AbstractServiceSession interface
    void headerFieldValue(const std::string &, const std::string &) override
    {}
    bool acceptContentLength(size_t) override {return false;}
    void headersComplete() override
    {}
    void body(const char *, size_t ) override
    {}
    void requestComplete() override
    {
        m_serverSession->responseStatus(200);
        m_serverSession->responseEndHeader(0);
        m_serverSession->responseComplete();
    }
    void writeResponse(Getodac::AbstractServerSession::Yield &yield) override
    {}
};

class TestSecureOnly : public BaseTestSession
{
public:
    explicit TestSecureOnly(Getodac::AbstractServerSession *serverSession)
        : BaseTestSession(serverSession)
    {
        if (!serverSession->isSecuredConnection())
            throw Getodac::ResponseStatusError{400, "Only secured connections allowed", {{"ErrorKey1","Value1"}, {"ErrorKey2","Value2"}}};
    }
    // AbstractServiceSession interface
    void requestComplete() override
    {
        m_serverSession->responseHeader("OkKey1", "value1");
        m_serverSession->responseHeader("OkKey2", "value2");
        m_serverSession->responseEndHeader(2);
    }
    void writeResponse(Getodac::AbstractServerSession::Yield &yield) override
    {
        m_serverSession->write(yield, "OK", 2);
        m_serverSession->responseComplete();
    }
};

struct TestThowFromHeaderFieldValue : public BaseTestSession
{
    explicit TestThowFromHeaderFieldValue(Getodac::AbstractServerSession *serverSession) : BaseTestSession(serverSession) {}
    void headerFieldValue(const std::string &, const std::string &) override
    {
        throw Getodac::ResponseStatusError{400, "Too many headers", {{"ErrorKey1","Value1"}, {"ErrorKey2","Value2"}}};
    }
};

struct TestThowFromHeadersComplete : public BaseTestSession
{
    explicit TestThowFromHeadersComplete(Getodac::AbstractServerSession *serverSession) : BaseTestSession(serverSession) {}

    void headersComplete() override
    {
        throw Getodac::ResponseStatusError{401, "What are you doing here?", {{"WWW-Authenticate","Basic realm=\"Restricted Area\""}, {"ErrorKey2","Value2"}}};
    }

};

struct TestThowFromBody : public BaseTestSession
{
    explicit TestThowFromBody(Getodac::AbstractServerSession *serverSession) : BaseTestSession(serverSession) {}

    void body(const char *, size_t ) override
    {
        throw Getodac::ResponseStatusError{400, "Body too big, lose some weight", {{"BodyKey1","Value1"}, {"BodyKey2","Value2"}}};
    }

};

class Test0 : public BaseTestSession
{
public:
    explicit Test0(Getodac::AbstractServerSession *serverSession)
        : BaseTestSession(serverSession)
    {}

    // ServiceSession interface
    void writeResponse(Getodac::AbstractServerSession::Yield &/*yield*/) override
    {
        assert(false);
    }
};


class Test100 : public BaseTestSession
{
public:
    explicit Test100(Getodac::AbstractServerSession *serverSession)
        : BaseTestSession(serverSession)
    {}

    // ServiceSession interface
    void requestComplete() override
    {
        m_serverSession->responseStatus(200);
        m_serverSession->responseEndHeader(test100response.size());
    }
    void writeResponse(Getodac::AbstractServerSession::Yield &yield) override
    {
        try {
            m_serverSession->write(yield, test100response.c_str(), test100response.size());
        } catch (const std::exception &e) {
            std::cerr << e.what() << std::endl;
        } catch (...) {
        }
        m_serverSession->responseComplete();
    }
};

class Test50M : public BaseTestSession
{
public:
    explicit Test50M(Getodac::AbstractServerSession *serverSession)
        : BaseTestSession(serverSession)
    {}

    // ServiceSession interface
    void requestComplete() override
    {
        m_serverSession->responseStatus(200);
        m_serverSession->responseEndHeader(test50mresponse.size());
    }

    void writeResponse(Getodac::AbstractServerSession::Yield &yield) override
    {
        try {
            m_serverSession->write(yield, test50mresponse.c_str(), test50mresponse.size());
        } catch (const std::exception &e) {
            std::cerr << e.what() << std::endl;
        } catch (...) {
        }
        m_serverSession->responseComplete();
    }
};

class Test50MS : public BaseTestSession
{
public:
    explicit Test50MS(Getodac::AbstractServerSession *serverSession)
        : BaseTestSession(serverSession)
    {}

    // ServiceSession interface
    void requestComplete() override
    {
        m_serverSession->responseStatus(200);
        m_serverSession->responseEndHeader(test50mresponse.size());
    }

    void writeResponse(Getodac::AbstractServerSession::Yield &yield) override
    {
        try {
            iovec vec[50];
            for (int i = 0; i < 50; ++i) {
                vec[i].iov_base = (void*)(test50mresponse.c_str() + 1024 * 1024 * i);
                vec[i].iov_len = 1024 * 1024;
            }
            m_serverSession->writev(yield, vec, 50);
        } catch (const std::exception &e) {
            std::cerr << e.what() << std::endl;
        } catch (...) {
        }
        m_serverSession->responseComplete();
    }
};

class Test50MChunked : public BaseTestSession
{
public:
    explicit Test50MChunked(Getodac::AbstractServerSession *serverSession)
        : BaseTestSession(serverSession)
    {}

    // ServiceSession interface
    void requestComplete() override
    {
        m_serverSession->responseStatus(200);
        m_serverSession->responseEndHeader(Getodac::ChunkedData);
    }

    void writeResponse(Getodac::AbstractServerSession::Yield &yield) override
    {
        uint32_t chunkSize = 1 + rand() % (1024 * 1024);
        chunkSize = std::min<uint32_t>(chunkSize, test50mresponse.size() - pos);
        writeChunkedData(yield, test50mresponse.c_str() + pos, chunkSize);
        if (!chunkSize)
            m_serverSession->responseComplete();
        else
            pos += chunkSize;
    }

private:
    uint32_t pos = 0;
};

class Test50MChunkedAtOnce : public BaseTestSession
{
public:
    explicit Test50MChunkedAtOnce(Getodac::AbstractServerSession *serverSession)
        : BaseTestSession(serverSession)
    {}

    // ServiceSession interface
    void requestComplete() override
    {
        m_serverSession->responseStatus(200);
        m_serverSession->responseEndHeader(Getodac::ChunkedData);
    }

    void writeResponse(Getodac::AbstractServerSession::Yield &yield) override
    {
        while(true) {
            uint32_t chunkSize = 1 + rand() % (1024 * 1024);
            chunkSize = std::min<uint32_t>(chunkSize, test50mresponse.size() - pos);
            writeChunkedData(yield, test50mresponse.c_str() + pos, chunkSize);
            if (!chunkSize) {
                m_serverSession->responseComplete();
                break;
            } else {
                pos += chunkSize;
            }
        }
    }

private:
    uint32_t pos = 0;
};


class TestRESTGET : public Getodac::AbstractRESTfulRouteGETSession<Getodac::AbstractSimplifiedServiceSession<>>
{
public:
    explicit TestRESTGET(Getodac::ParsedRoute &&resources, Getodac::AbstractServerSession *serverSession)
        : Getodac::AbstractRESTfulRouteGETSession<Getodac::AbstractSimplifiedServiceSession<>>(std::move(resources), serverSession)
    {}

    void writeResponse(Getodac::OStream &stream) override
    {
        stream << "Got " << m_parsedRoute.capturedResources.size() << " captured resources\n";
        stream << "and " << m_parsedRoute.queryStrings.size() << " queries\n";
        stream << "All methods but OPTIONS " << m_parsedRoute.allButOPTIONSNodeMethods << " \n";
        for (const auto &resource : m_parsedRoute.capturedResources) {
            stream << "Resource name: " << resource.first << "  value: " << resource.second << std::endl;
        }
        for (const auto &query : m_parsedRoute.queryStrings) {
            stream << "Query name: " << query.first << "  value: " << query.second << std::endl;
        }
    }
};

class EchoTest : public Getodac::AbstractServiceSession
{
public:
    explicit EchoTest(Getodac::AbstractServerSession *serverSession)
        : Getodac::AbstractServiceSession(serverSession)
    {}

    // ServiceSession interface
    // AbstractServiceSession interface
    void headerFieldValue(const std::string &field, const std::string &value) override
    {
        m_headers.emplace_back(std::make_pair(field, value));
    }
    bool acceptContentLength(size_t length) override
    {
        contentLength = length;
        return true;
    }
    void headersComplete() override {}
    void body(const char *data, size_t length) override
    {
        m_body.append(data, length);
    }
    void requestComplete() override
    {
        m_serverSession->responseStatus(200);
        m_serverSession->responseEndHeader(Getodac::ChunkedData);
    }
    void writeResponse(Getodac::AbstractServerSession::Yield &yield) override
    {
        Getodac::OStreamBuffer streamBuffer{this, yield, true};
        Getodac::OStream stream(streamBuffer);
        stream << "~~~~ ContentLength: " << contentLength << std::endl;
        stream << "~~~~ Headers:\n";
        for (const auto &kv : m_headers)
            stream << kv.first << " : " << kv.second << std::endl;
        stream << "~~~~ Body:\n" << m_body;
    }
private:
    std::vector<std::pair<std::string, std::string>> m_headers;
    size_t contentLength = 0;
    std::string m_body;
};

} // namespace

PLUGIN_EXPORT std::shared_ptr<Getodac::AbstractServiceSession> createSession(Getodac::AbstractServerSession *serverSession, const std::string &url, const std::string &method)
{
    if (url == "/test100")
        return std::make_shared<Test100>(serverSession);

    if (url == "/test50m")
        return std::make_shared<Test50M>(serverSession);

    if (url == "/test50mChunked")
        return std::make_shared<Test50MChunked>(serverSession);

    if (url == "/test50mChunkedAtOnce")
        return std::make_shared<Test50MChunkedAtOnce>(serverSession);

    if (url == "/test50ms")
        return std::make_shared<Test50MS>(serverSession);

    if (url == "/test0")
        return std::make_shared<Test0>(serverSession);

    if (url == "/echoTest")
        return std::make_shared<EchoTest>(serverSession);

    if (url == "/secureOnly")
        return std::make_shared<TestSecureOnly>(serverSession);

    if (url == "/testThowFromHeaderFieldValue")
        return std::make_shared<TestThowFromHeaderFieldValue>(serverSession);

    if (url == "/testThowFromHeadersComplete")
        return std::make_shared<TestThowFromHeadersComplete>(serverSession);

    if (url == "/testThowFromBody")
        return std::make_shared<TestThowFromBody>(serverSession);

    return s_testRootRestful.createHandler(url, method, serverSession);
}

PLUGIN_EXPORT bool initPlugin(const std::string &/*confDir*/)
{
    for (int i = 0; i < 50 * 1024 * 1024; ++i)
        test50mresponse += char(33 + (i % 93));

    s_testRootRestful.createRoute("customers")
            ->addMethodHandler("GET", Getodac::sessionHandler<TestRESTGET>());
    s_testRootRestful.createRoute("customers/{customerId}")
            ->addMethodHandler("GET", Getodac::sessionHandler<TestRESTGET>());
    s_testRootRestful.createRoute("customers/{customerId}/licenses")
            ->addMethodHandler("GET", Getodac::sessionHandler<TestRESTGET>());
    s_testRootRestful.createRoute("customers/{customerId}/licenses/{licenseId}")
            ->addMethodHandler("GET", Getodac::sessionHandler<TestRESTGET>());
    return true;
}

PLUGIN_EXPORT uint32_t pluginOrder()
{
    return 9999999;
}

PLUGIN_EXPORT void destoryPlugin()
{
}
