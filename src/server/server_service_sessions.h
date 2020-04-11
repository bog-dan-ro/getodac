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

#ifndef SERVER_SERVICE_SESSIONS_H
#define SERVER_SERVICE_SESSIONS_H

#include <string>

#include <getodac/abstract_server_session.h>
#include <getodac/abstract_service_session.h>

namespace ServerSessions {
    std::shared_ptr<Getodac::AbstractServiceSession> createSession(Getodac::AbstractServerSession *serverSession, const std::string &url, const std::string &method);
} // namespace Server

#endif // SERVER_SERVICE_SESSIONS_H
