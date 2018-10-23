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

#include "server_service_sessions.h"
#include "server.h"

#include <iostream>
#include <sstream>

namespace ServerSessions {

/// ServerStatus class is used to show the current server status
class ServerStatus : public Getodac::AbstractServiceSession
{
public:
    explicit ServerStatus(Getodac::AbstractServerSession *serverSession)
        : Getodac::AbstractServiceSession(serverSession)
    {}

    // ServiceSession interface
    void headerFieldValue(const std::string &, const std::string &) override {}
    void headersComplete() override {}
    void body(const char *, size_t) override {}
    void requestComplete() override
    {
        m_serverSession->responseStatus(200);
        std::ostringstream response;

        auto server = Getodac::Server::instance();
        auto activeSessions = server->activeSessions();

        // the peakSessions is updated slowly
        auto peak = std::max(server->peakSessions(), activeSessions);

        auto seconds = server->uptime().count();
        auto days = seconds / (60 * 60 * 24);
        seconds  -= days * (60 * 60 * 24);
        auto hours = seconds / (60 * 60);
        seconds  -= hours * (60 * 60 * 24);
        auto minutes = seconds / 60;
        seconds  -= minutes * 60;

        auto servedSessions = server->servedSessions();

        response << "Active sessions: " << activeSessions << std::endl
                 << "Sessions peak: " << peak << std::endl
                 << "Uptime: " << days << " days, " << hours << " hours, " << minutes << " minutes and " << seconds << " seconds" << std::endl
                 << "Serverd sessions: " << servedSessions << std::endl;

        m_response = std::move(response.str());

        m_serverSession->responseHeader("Refresh", "5");
        m_serverSession->responseEndHeader(m_response.size());
    }

    void writeResponse(Getodac::AbstractServerSession::Yield &yield) override
    {
        try {
            m_serverSession->write(yield, m_response.c_str(), m_response.size());
        } catch (const std::exception &e) {
            std::cerr << e.what() << std::endl;
        } catch (...) {
        }
        m_serverSession->responseComplete();
    }

private:
    std::string m_response;
};

std::shared_ptr<Getodac::AbstractServiceSession> createSession(Getodac::AbstractServerSession *serverSession, const std::string &url, const std::string &/*method*/)
{
    if (url == "/server_status")
        return std::make_shared<ServerStatus>(serverSession);

    return std::shared_ptr<Getodac::AbstractServiceSession>();
}

} // namespace ServerSessions
