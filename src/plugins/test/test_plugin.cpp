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
#include <getodac/restful.h>

#include <iostream>
#include <mutex>

namespace {

const std::string test100response("100XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX");
Getodac::SpinLock test50mresponse_lock;
std::string test50mresponse;

Getodac::RESTful<std::shared_ptr<Getodac::AbstractServiceSession>> s_testResful("/test/rest/v1/");

class Test0 : public Getodac::AbstractServiceSession
{
public:
    Test0(Getodac::AbstractServerSession *serverSession, const std::string &url, const std::string &method)
        : Getodac::AbstractServiceSession(serverSession)
    {
        std::cout << url << std::endl << method << std::endl;
    }

    // ServiceSession interface
    void headerFieldValue(const std::string &, const std::string &) override {}
    void headersComplete() override {}
    void body(const char *, size_t) override {}
    void requestComplete() override
    {
        m_serverSession->responseStatus(200);
        m_serverSession->responseEndHeader(0);
        m_serverSession->responseComplete();
    }
    void writeResponse(Getodac::AbstractServerSession::Yield &/*yield*/) override
    {
        assert(false);
    }
};


class Test100 : public Getodac::AbstractServiceSession
{
public:
    Test100(Getodac::AbstractServerSession *serverSession)
        : Getodac::AbstractServiceSession(serverSession)
    {}

    // ServiceSession interface
    void headerFieldValue(const std::string &, const std::string &) override {}
    void headersComplete() override {}
    void body(const char *, size_t) override {}
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

class Test50M : public Getodac::AbstractServiceSession
{
public:
    Test50M(Getodac::AbstractServerSession *serverSession)
        : Getodac::AbstractServiceSession(serverSession)
    {
        std::unique_lock<Getodac::SpinLock> lock(test50mresponse_lock);
        if (test50mresponse.empty())
            for (int i = 0; i < 500000; ++i)
                test50mresponse += test100response;
    }

    // ServiceSession interface
    void headerFieldValue(const std::string &, const std::string &) override {}
    void headersComplete() override {}
    void body(const char *, size_t) override {}
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

class Test50MS : public Getodac::AbstractServiceSession
{
public:
    Test50MS(Getodac::AbstractServerSession *serverSession)
        : Getodac::AbstractServiceSession(serverSession)
    {
        std::unique_lock<Getodac::SpinLock> lock(test50mresponse_lock);
        if (test50mresponse.empty())
            for (int i = 0; i < 500000; ++i)
                test50mresponse += test100response;
    }

    // ServiceSession interface
    void headerFieldValue(const std::string &, const std::string &) override {}
    void headersComplete() override {}
    void body(const char *, size_t) override {}
    void requestComplete() override
    {
        m_serverSession->responseStatus(200);
        m_serverSession->responseEndHeader(50000000);
    }
    void writeResponse(Getodac::AbstractServerSession::Yield &yield) override
    {
        try {
            iovec vec[500];
            for (int i = 0; i < 500; ++i) {
                vec[i].iov_base = (void*)test50mresponse.c_str();
                vec[i].iov_len = 100000;
            }
            m_serverSession->writev(yield, vec, 500);
        } catch (const std::exception &e) {
            std::cerr << e.what() << std::endl;
        } catch (...) {
        }
        m_serverSession->responseComplete();
    }
};

class TestRESTGET : public Getodac::AbstractServiceSession
{
public:
    TestRESTGET(Getodac::AbstractServerSession *serverSession, Getodac::Resources &&resources)
        : Getodac::AbstractServiceSession(serverSession)
        , m_resources(std::move(resources))
    {}

    // ServiceSession interface
    void headerFieldValue(const std::string &, const std::string &) override {}
    void headersComplete() override {}
    void body(const char *, size_t) override {}
    void requestComplete() override
    {
        m_serverSession->responseStatus(200);
        m_serverSession->responseEndHeader(Getodac::ChuckedData);
    }
    void writeResponse(Getodac::AbstractServerSession::Yield &yield) override
    {
        std::stringstream stream;
        stream << "Got " << m_resources.size() << " resources\n";
        writeChunkedData(yield, stream.str());
        stream.str({});
        for (const auto &resource : m_resources) {
            stream << "Resource name: " << resource .name << " \nResource value: " << resource.value << std::endl;
            for (const auto &query : resource.queryStrings)
                stream << "    " << query.first << " = " << query.second << std::endl;
            writeChunkedData(yield, stream.str());
            stream.str({});
        }
        writeChunkedData(yield, nullptr, 0);
        m_serverSession->responseComplete();
    }

private:
    Getodac::Resources m_resources;
};

} // namespace

PLUGIN_EXPORT std::shared_ptr<Getodac::AbstractServiceSession> createSession(Getodac::AbstractServerSession *serverSession, const std::string &url, const std::string &method)
{
    if (url == "/test100")
        return std::make_shared<Test100>(serverSession);

    if (url == "/test50m")
        return std::make_shared<Test50M>(serverSession);

    if (url == "/test50ms")
        return std::make_shared<Test50MS>(serverSession);

    if (url == "/test0")
        return std::make_shared<Test0>(serverSession, url, method);

    if (s_testResful.canHanldle(url, method))
            return s_testResful.parse(serverSession, url, method);

    return std::shared_ptr<Getodac::AbstractServiceSession>();
}

PLUGIN_EXPORT bool initPlugin()
{
    auto getMethod = [](Getodac::AbstractServerSession *serverSession, Getodac::Resources &&resources) {
        return std::make_shared<TestRESTGET>(serverSession, std::move(resources));
    };
    s_testResful.setMethodCallback("GET", getMethod, {"customers", "orders"});
    return true;
}

PLUGIN_EXPORT void destoryPlugin()
{
}
