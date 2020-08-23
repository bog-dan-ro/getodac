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

#include "server_service_sessions.h"
#include "server.h"
#include "server_logger.h"

#include <chrono>
#include <iostream>
#include <sstream>

#include <getodac/http.h>

using namespace std::chrono_literals;
namespace server_sessions {


static void write_response(Getodac::abstract_stream& stream, Getodac::request& req)
{
    bool can_write_error = true;
    try {
        stream >> req;
        Getodac::response res{200};
        {
            res["Refresh"] = "5";
            std::ostringstream response;

            auto server = Getodac::server::instance();
            auto activeSessions = server->active_sessions();

            // the peakSessions is updated slowly
            auto peak = std::max(server->peak_sessions(), activeSessions);

            auto seconds = server->uptime().count();
            auto days = seconds / (60 * 60 * 24);
            seconds  -= days * (60 * 60 * 24);
            auto hours = seconds / (60 * 60);
            seconds  -= hours * (60 * 60);
            auto minutes = seconds / 60;
            seconds  -= minutes * 60;

            auto servedSessions = server->served_sessions();
            response << "Active sessions: " << activeSessions << std::endl
                     << "Sessions peak: " << peak << std::endl
                     << "Uptime: " << days << " days, " << hours << " hours, " << minutes << " minutes and " << seconds << " seconds" << std::endl
                     << "Serverd sessions: " << servedSessions << std::endl;
            res.body(response.str());
        }
        can_write_error = false;
        stream << res;
    } catch (const Getodac::response &res) {
        if (can_write_error)
            stream << res;
        WARNING(Getodac::server_logger) << res.status_code() << " " << res.body();
    } catch (const std::error_code &ec) {
        if (can_write_error)
            stream << Getodac::response{500, ec.message()};
        WARNING(Getodac::server_logger) << ec.message();
    } catch (const std::exception &e) {
        if (can_write_error)
            stream << Getodac::response{500, e.what()};
        WARNING(Getodac::server_logger) << e.what();
    } catch (...) {
        if (can_write_error)
            stream << Getodac::response{500};
        WARNING(Getodac::server_logger) << "Unkown error";
    }
}

Getodac::HttpSession create_session(const Getodac::request &req)
{
    if (req.url() == "/server_status" && req.method() == "GET")
        return &server_sessions::write_response;
    return {};
}

} // namespace ServerSessions
