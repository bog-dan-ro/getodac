/*
    Copyright (C) 2022, BogDan Vatra <bogdan@kde.org>

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

#include "serverservicesessions.h"
#include "server.h"
#include "serverlogger.h"

#include <chrono>
#include <iostream>
#include <sstream>

#include <dracon/http.h>

using namespace std::chrono_literals;
namespace ServerSessions {


static void writeResponse(Dracon::AbstractStream& stream, Dracon::Request& req)
{
    bool canWriteError = true;
    try {
        stream >> req;
        Dracon::Response res{200};
        {
            res["Refresh"] = "5";
            std::ostringstream response;

            const auto &server = Getodac::Server::instance();
            auto activeSessions = server.activeSessions();

            // the peakSessions is updated slowly
            auto peak = std::max(server.peakSessions(), activeSessions);

            auto seconds = server.uptime().count();
            auto days = seconds / (60 * 60 * 24);
            seconds  -= days * (60 * 60 * 24);
            auto hours = seconds / (60 * 60);
            seconds  -= hours * (60 * 60);
            auto minutes = seconds / 60;
            seconds  -= minutes * 60;

            auto servedSessions = server.servedSessions();
            response << "Active sessions: " << activeSessions << std::endl
                     << "Sessions peak: " << peak << std::endl
                     << "Uptime: " << days << " days, " << hours << " hours, " << minutes << " minutes and " << seconds << " seconds" << std::endl
                     << "Serverd sessions: " << servedSessions << std::endl;
            res.setBody(response.str());
        }
        canWriteError = false;
        stream << res;
    } catch (const Dracon::Response &res) {
        if (canWriteError)
            stream << res;
        WARNING(Getodac::ServerLogger) << res.statusCode() << " " << res.body();
    } catch (const std::error_code &ec) {
        if (canWriteError)
            stream << Dracon::Response{500, ec.message()};
        WARNING(Getodac::ServerLogger) << ec.message();
    } catch (const std::exception &e) {
        if (canWriteError)
            stream << Dracon::Response{500, e.what()};
        WARNING(Getodac::ServerLogger) << e.what();
    } catch (...) {
        if (canWriteError)
            stream << Dracon::Response{500};
        WARNING(Getodac::ServerLogger) << "Unkown error";
    }
}

Dracon::HttpSession createSession(const Dracon::Request &req)
{
    if (req.url() == "/server_status" && req.method() == "GET")
        return &ServerSessions::writeResponse;
    return {};
}

} // namespace ServerSessions
