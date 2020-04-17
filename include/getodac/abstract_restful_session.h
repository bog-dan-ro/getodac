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

    AGPL EXCEPTION:
    The AGPL license applies only to this file itself.

    As a special exception, the copyright holders of this file give you permission
    to use it, regardless of the license terms of your work, and to copy and distribute
    them under terms of your choice.
    If you do any changes to this file, these changes must be published under AGPL.

*/

#pragma once

#include "abstract_service_session.h"
#include "restful.h"

namespace Getodac {

using RESTfulResourceType = RESTfulResource<std::shared_ptr<AbstractServiceSession>, AbstractServerSession*>;

template <typename T>
RESTfulResourceMethodCreator<std::shared_ptr<T>, AbstractServerSession*> sessionCreator()
{
    return [](ParsedUrl &&parsedUrl, AbstractServerSession* session) -> std::shared_ptr<T> {
        return std::make_shared<T>(std::move(parsedUrl), session);
    };
}

template <typename BaseClass = AbstractServiceSession>
class AbstractRESTfulBaseSession : public BaseClass
{
public:
    explicit AbstractRESTfulBaseSession(ParsedUrl &&parsedUrl, AbstractServerSession *serverSession)
        : BaseClass(serverSession)
        , m_parsedUrl(std::move(parsedUrl))
    {}

protected:
    ParsedUrl m_parsedUrl;
};

template <typename BaseClass>
class AbstractRESTfulGETSession : public AbstractRESTfulBaseSession<BaseClass>
{
public:
    explicit AbstractRESTfulGETSession(ParsedUrl &&resources, AbstractServerSession *serverSession)
        : AbstractRESTfulBaseSession<BaseClass>(std::move(resources), serverSession)
    {}

    AbstractSimplifiedServiceSession<>::ResponseHeaders responseHeaders() override
    {
        auto response = AbstractRESTfulBaseSession<BaseClass>::responseHeaders();
        if (response.status != 200)
            return response;
        response.contentLenght = Getodac::ChunkedData;
        return response;
    }

    // Usually GET and Delete methods don't expect any body
    bool acceptContentLength(size_t length) override
    {
        (void)length; return false;
    }
    void body(const char *data, size_t length) override
    {
        (void)data;
        (void)length;
        throw ResponseStatusError(400, "Unexpected body data");
    }
};

template <typename BaseClass>
class AbstractRESTfulDELETESession : public AbstractRESTfulGETSession<BaseClass>
{
public:
    explicit AbstractRESTfulDELETESession(ParsedUrl &&resources, AbstractServerSession *serverSession)
        : AbstractRESTfulGETSession<BaseClass>(std::move(resources), serverSession)
    {
    }

    /// Usually DELETE operations doesn't have to write any response body
    /// override is needed
    void writeResponse(Getodac::OStream &) override {}
};

template <typename BaseClass>
class RESTfulOPTIONSSession : public AbstractRESTfulGETSession<BaseClass>
{
public:
    explicit RESTfulOPTIONSSession(ParsedUrl &&resources, AbstractServerSession *serverSession)
        : AbstractRESTfulGETSession<BaseClass>(std::move(resources), serverSession)
    {
        AbstractRESTfulGETSession<BaseClass>::m_requestHeadersFilter.acceptedHeades.emplace("Access-Control-Request-Headers");
    }

    AbstractSimplifiedServiceSession<>::ResponseHeaders responseHeaders() override
    {
        auto response = AbstractRESTfulGETSession<BaseClass>::responseHeaders();
        if (response.status != 200)
            return response;
        response.headers.emplace("Access-Control-Allow-Methods", AbstractRESTfulGETSession<BaseClass>::m_parsedUrl.allButOPTIONSNodeMethods);
        auto it = AbstractRESTfulGETSession<BaseClass>::m_requestHeaders.find("Access-Control-Request-Headers");
        if (it != AbstractRESTfulGETSession<BaseClass>::m_requestHeaders.end())
             response.headers.emplace("Access-Control-Allow-Headers", it->second);
        return response;
    }

    void writeResponse(Getodac::OStream &) override {}
};


using RESTfulRouterType = RESTfulRouter<std::shared_ptr<AbstractServiceSession>, AbstractServerSession*>;

template <typename T>
RESTfulRouteMethodHandler<std::shared_ptr<T>, AbstractServerSession*> sessionHandler()
{
    return [](ParsedRoute &&parsedRoute, AbstractServerSession* session) -> std::shared_ptr<T> {
        return std::make_shared<T>(std::move(parsedRoute), session);
    };
}

template <typename BaseClass = AbstractServiceSession>
class AbstractRESTfulRouteBaseSession : public BaseClass
{
public:
    explicit AbstractRESTfulRouteBaseSession(ParsedRoute &&parsedRoute, AbstractServerSession *serverSession)
        : BaseClass(serverSession)
        , m_parsedRoute(std::move(parsedRoute))
    {}

protected:
    ParsedRoute m_parsedRoute;
};

template <typename BaseClass>
class AbstractRESTfulRouteGETSession : public AbstractRESTfulRouteBaseSession<BaseClass>
{
public:
    explicit AbstractRESTfulRouteGETSession(ParsedRoute &&resources, AbstractServerSession *serverSession)
        : AbstractRESTfulRouteBaseSession<BaseClass>(std::move(resources), serverSession)
    {}

    AbstractSimplifiedServiceSession<>::ResponseHeaders responseHeaders() override
    {
        auto response = AbstractRESTfulRouteBaseSession<BaseClass>::responseHeaders();
        if (response.status != 200)
            return response;
        response.contentLenght = Getodac::ChunkedData;
        return response;
    }

    // Usually GET and Delete methods don't expect any body
    bool acceptContentLength(size_t length) override
    {
        (void)length; return false;
    }
    void body(const char *data, size_t length) override
    {
        (void)data;
        (void)length;
        throw ResponseStatusError(400, "Unexpected body data");
    }
};

template <typename BaseClass>
class AbstractRESTfulRouteDELETESession : public AbstractRESTfulRouteGETSession<BaseClass>
{
public:
    explicit AbstractRESTfulRouteDELETESession(ParsedRoute &&resources, AbstractServerSession *serverSession)
        : AbstractRESTfulRouteGETSession<BaseClass>(std::move(resources), serverSession)
    {
    }

    /// Usually DELETE operations doesn't have to write any response body
    /// override is needed
    void writeResponse(Getodac::OStream &) override {}
};

template <typename BaseClass>
class RESTfulRouteOPTIONSSession : public AbstractRESTfulRouteGETSession<BaseClass>
{
public:
    explicit RESTfulRouteOPTIONSSession(ParsedRoute &&resources, AbstractServerSession *serverSession)
        : AbstractRESTfulRouteGETSession<BaseClass>(std::move(resources), serverSession)
    {
        AbstractRESTfulRouteGETSession<BaseClass>::m_requestHeadersFilter.acceptedHeades.emplace("Access-Control-Request-Headers");
    }

    AbstractSimplifiedServiceSession<>::ResponseHeaders responseHeaders() override
    {
        auto response = AbstractRESTfulRouteGETSession<BaseClass>::responseHeaders();
        if (response.status != 200)
            return response;
        response.headers.emplace("Access-Control-Allow-Methods", AbstractRESTfulRouteGETSession<BaseClass>::m_parsedUrl.allButOPTIONSNodeMethods);
        auto it = AbstractRESTfulRouteGETSession<BaseClass>::m_requestHeaders.find("Access-Control-Request-Headers");
        if (it != AbstractRESTfulRouteGETSession<BaseClass>::m_requestHeaders.end())
             response.headers.emplace("Access-Control-Allow-Headers", it->second);
        return response;
    }

    void writeResponse(Getodac::OStream &) override {}
};

} // namespace Getodac
