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
#include <getodac/abstract_restfull_session.h>

#include <cassert>
#include <iostream>
#include <mutex>

namespace {

const std::string test100response{"100XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX"};
std::string test50mresponse;
const std::string test100Key{"/test100"};
const std::string test0Key{"/test0"};
const std::string test50MKey{"/test50m"};
const std::string test50MSKey{"/test50ms"};

Getodac::RESTfullResourceType s_testRootRestful("/test/rest/v1/");

class Test0 : public Getodac::AbstractServiceSession
{
public:
    Test0(Getodac::AbstractServerSession *serverSession, const std::string &, const std::string &)
        : Getodac::AbstractServiceSession(serverSession)
    {}

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
    {}

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
    {}

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

class TestRESTGET : public Getodac::AbstractRestfullGETSession<Getodac::AbstractSimplifiedServiceSession>
{
public:
    TestRESTGET(Getodac::ParsedUrl &&resources, Getodac::AbstractServerSession *serverSession)
        : Getodac::AbstractRestfullGETSession<Getodac::AbstractSimplifiedServiceSession>(std::move(resources), serverSession)
    {}

    void writeResponse(std::ostream &stream) override
    {
        stream << "Got " << m_parsedUrl.resources.size() << " resources\n";
        stream << "and " << m_parsedUrl.queryStrings.size() << " queries\n";
        stream << "All methods but OPTIONS " << m_parsedUrl.allButOPTIONSNodeMethods << " \n";
        for (const auto &resource : m_parsedUrl.resources) {
            stream << "Resource name: " << resource.first << "  value: " << resource.second << std::endl;
        }
        for (const auto &query : m_parsedUrl.queryStrings) {
            stream << "Query name: " << query.first << "  value: " << query.second << std::endl;
        }
    }
};

} // namespace

PLUGIN_EXPORT std::shared_ptr<Getodac::AbstractServiceSession> createSession(Getodac::AbstractServerSession *serverSession, const std::string &url, const std::string &method)
{
    if (url == test100Key)
        return std::make_shared<Test100>(serverSession);

    if (url == test50MKey)
        return std::make_shared<Test50M>(serverSession);

    if (url == test50MSKey)
        return std::make_shared<Test50MS>(serverSession);

    if (url == test0Key)
        return std::make_shared<Test0>(serverSession, url, method);

    return s_testRootRestful.create(url, method, serverSession);
}

PLUGIN_EXPORT bool initPlugin(const std::string &/*confDir*/)
{
    test50mresponse.reserve(100 * 500000);
    for (int i = 0; i < 500000; ++i)
        test50mresponse += test100response;

    Getodac::RESTfullResourceType customersResource{"customers"};
    customersResource.addMethodCreator("GET", Getodac::sessionCreator<TestRESTGET>());
    s_testRootRestful.addSubResource(customersResource);
    return true;
}

PLUGIN_EXPORT uint32_t pluginOrder()
{
    return 9999999;
}

PLUGIN_EXPORT void destoryPlugin()
{
}
