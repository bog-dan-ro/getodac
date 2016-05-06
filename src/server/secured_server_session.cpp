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

#include "secured_server_session.h"
#include "server.h"
namespace Getodac {

SecuredServerSession::SecuredServerSession(SessionsEventLoop *eventLoop, int sock)
    : ServerSession(eventLoop, sock)
{
    if (!(m_SSL = SSL_new(Server::instance()->sslContext())))
        throw std::runtime_error(ERR_error_string(ERR_get_error(), nullptr));

    if (!SSL_set_fd(m_SSL, sock))
        throw std::runtime_error(ERR_error_string(SSL_get_error(m_SSL, 0), nullptr));

    SSL_set_accept_state(m_SSL);
}

SecuredServerSession::~SecuredServerSession()
{
    if (m_SSL)
        SSL_free(m_SSL);
}

} // namespace Getodac
