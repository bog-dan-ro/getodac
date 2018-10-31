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

using RESTfullResourceType = RESTfullResource<std::shared_ptr<AbstractServiceSession>, AbstractServerSession*>;

template <typename T>
RESTfullResourceMethodCreator<std::shared_ptr<T>, AbstractServerSession*> sessionCreator()
{
    return [](ParsedUrl &&parsedUrl, AbstractServerSession* session) -> std::shared_ptr<T> {
        return std::make_shared<T>(std::move(parsedUrl), session);
    };
}

template <typename BaseClass>
class AbstractRestfullBaseSession : public BaseClass
{
public:
    explicit AbstractRestfullBaseSession(ParsedUrl &&parsedUrl, AbstractServerSession *serverSession)
        : BaseClass(serverSession)
        , m_parsedUrl(std::move(parsedUrl))
    {}

protected:
    ParsedUrl m_parsedUrl;
};

template <typename BaseClass>
class AbstractRestfullGETSession : public AbstractRestfullBaseSession<BaseClass>
{
public:
    explicit AbstractRestfullGETSession(ParsedUrl &&resources, AbstractServerSession *serverSession)
        : AbstractRestfullBaseSession<BaseClass>(std::move(resources), serverSession)
    {}

    AbstractSimplifiedServiceSession::ResponseHeaders responseHeaders() override
    {
        auto response = AbstractRestfullBaseSession<BaseClass>::responseHeaders();
        if (response.status != 200)
            return response;
        response.contentLenght = Getodac::ChunckedData;
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
class AbstractRestfullDELETESession : public AbstractRestfullGETSession<BaseClass>
{
public:
    explicit AbstractRestfullDELETESession(ParsedUrl &&resources, AbstractServerSession *serverSession)
        : AbstractRestfullGETSession<BaseClass>(std::move(resources), serverSession)
    {
    }

    /// Usually DELETE operations doesn't have to write any response body
    /// override is needed
    void writeResponse(Getodac::OStream &) override {}
};

template <typename BaseClass>
class RestfullOPTIONSSession : public AbstractRestfullGETSession<BaseClass>
{
public:
    explicit RestfullOPTIONSSession(ParsedUrl &&resources, AbstractServerSession *serverSession)
        : AbstractRestfullGETSession<BaseClass>(std::move(resources), serverSession)
    {
        AbstractRestfullGETSession<BaseClass>::m_requestHeadersFilter.acceptedHeades.emplace("Access-Control-Request-Headers");
    }

    AbstractSimplifiedServiceSession::ResponseHeaders responseHeaders() override
    {
        auto response = AbstractRestfullGETSession<BaseClass>::responseHeaders();
        if (response.status != 200)
            return response;
        response.headers.emplace("Access-Control-Allow-Methods", AbstractRestfullGETSession<BaseClass>::m_parsedUrl.allButOPTIONSNodeMethods);
        auto it = AbstractRestfullGETSession<BaseClass>::m_requestHeaders.find("Access-Control-Request-Headers");
        if (it != AbstractRestfullGETSession<BaseClass>::m_requestHeaders.end())
             response.headers.emplace("Access-Control-Allow-Headers", it->second);
        return response;
    }

    void writeResponse(Getodac::OStream &) override {}
};

} // namespace Getodac
