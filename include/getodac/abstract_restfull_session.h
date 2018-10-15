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

#ifndef ABSTRACTRESTFULLSERVICESESSION_H
#define ABSTRACTRESTFULLSERVICESESSION_H

#include <getodac/abstract_service_session.h>
#include <getodac/restful.h>

namespace Getodac {

class AbstractRestfullServiceSession : public AbstractServiceSession
{
public:
    enum class DataType {
        Json,
        XML,
        ProtocolBuffers,
        Flatbuffers
    };

public:
    explicit AbstractRestfullServiceSession(Getodac::AbstractServerSession *serverSession, ParsedUrl &&resources)
        : AbstractServiceSession(serverSession)
        , m_restData(std::move(resources))
    {}

    void headerFieldValue(const std::string &field, const std::string &value) override
    {
        auto type = [&value] {
            if (value == "application/json" || value == "*/*")
                return DataType::Json;
            else if (value == "application/xml")
                return DataType::XML;
            else if (value == "application/x-protobuf")
                return DataType::ProtocolBuffers;
            else if (value == "application/x-flatbuf")
                return DataType::Flatbuffers;
            throw Getodac::ResponseStatusError(400, "Unknown content type");
        };
        if (field == "content-type")
            m_requestDataType = type();
        else if (field == "accept")
            m_responseDataType = type();
    }
    void headersComplete() override {}

    inline void setResponseHeaders()
    {
        auto type = [this] {
            switch (m_responseDataType) {
            case DataType::Json:
                return "application/json";
            case DataType::XML:
                return "application/xml";
            case DataType::ProtocolBuffers:
                return "application/x-protobuf";
            case DataType::Flatbuffers:
                return "application/x-flatbuf";
            }
            throw Getodac::ResponseStatusError(400, "Unknown content type");
        };
        m_serverSession->responseHeader("Access-Control-Allow-Origin", "*");
        m_serverSession->responseHeader("Content-Type", type());
    }

protected:
    DataType m_requestDataType = DataType::Json;
    DataType m_responseDataType = DataType::Json;
    ParsedUrl m_restData;
};

class AbstractRestfullGETSession : public AbstractRestfullServiceSession
{
public:
    explicit AbstractRestfullGETSession(Getodac::AbstractServerSession *serverSession, ParsedUrl &&resources)
        : AbstractRestfullServiceSession(serverSession, std::move(resources))
    {}

    void requestComplete() override
    {
        m_serverSession->responseStatus(200);
        setResponseHeaders();
        m_serverSession->responseEndHeader(ChuckedData);
    }

    // Usually GET and Delete methods don't expect any body
    void body(const char *data, size_t length) override
    {
        (void)data;
        (void)length;
        throw Getodac::ResponseStatusError(400, "Unexpected body data");
    }
};

class AbstractRestfullDELETESession : public AbstractRestfullGETSession
{
public:
    explicit AbstractRestfullDELETESession(Getodac::AbstractServerSession *serverSession, ParsedUrl &&resources)
        : AbstractRestfullGETSession(serverSession, std::move(resources))
    {
#ifndef NO_SSL
        if (!m_serverSession->isSecuredConnection())
            throw Getodac::ResponseStatusError(401, "Unsecure connection");
#endif
    }

    /// Usually DELETE operations doesn't have to write any response body
    /// override is needed
    void writeResponse(Getodac::AbstractServerSession::Yield &yield) override {(void)yield;}
};

template<const char* OPTIONS = "GET, POST, DELETE, PUT, PATCH">
class RestfullOPTIONSSession : public AbstractRestfullGETSession
{
public:
    explicit RestfullOPTIONSSession(Getodac::AbstractServerSession *serverSession, ParsedUrl &&resources)
        : AbstractRestfullGETSession(serverSession, std::move(resources))
    {}

    void headerFieldValue(const std::string &field, const std::string &value) override
    {
        if (field == "Access-Control-Request-Headers")
            m_allowHeaders = value;
        else
            AbstractRestfullGETSession::headerFieldValue(field, value);
    }

    void writeResponse(Getodac::AbstractServerSession::Yield &yield) override {(void)yield;}
    void requestComplete() override
    {
        m_serverSession->responseStatus(200);
        setResponseHeaders();
        m_serverSession->responseHeader("Access-Control-Allow-Methods", OPTIONS);
        if (!m_allowHeaders.empty())
             m_serverSession->responseHeader("Access-Control-Allow-Headers", m_allowHeaders);
        m_serverSession->responseEndHeader(0);
        m_serverSession->responseComplete();
    }

protected:
    std::string m_allowHeaders;
};

/// PPP stands for post, put, patch. These oprerations have a request body which must be
/// handled properly
class AbstractRestfullPPPSession : public AbstractRestfullServiceSession
{
public:
    explicit AbstractRestfullPPPSession(Getodac::AbstractServerSession *serverSession, ParsedUrl &&resources)
        : AbstractRestfullServiceSession(serverSession, std::move(resources))
    {
#ifndef NO_SSL
        if (!m_serverSession->isSecuredConnection())
            throw Getodac::ResponseStatusError(401, "Unsecure connection");
#endif
    }

    void finishSession(uint32_t statusCode)
    {
        m_serverSession->responseStatus(statusCode);
        m_serverSession->responseEndHeader(0);
        m_serverSession->responseComplete();
    }

    // Post, put, patch, etc. operations don't usually need to send any data
    void writeResponse(AbstractServerSession::Yield &yield) override {(void)yield;}
};


} // namespace Getodac

#endif // ABSTRACTRESTFULLSERVICESESSION_H
