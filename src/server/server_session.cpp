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

#include "server_session.h"

namespace Getodac {

basic_server_session::basic_server_session(Getodac::sessions_event_loop *event_loop, int sock, const sockaddr_storage &sock_addr, uint32_t order)
    : m_sock(sock)
    , m_order(order)
    , m_peer_addr(sock_addr)
    , m_event_loop(event_loop)
{
    server::instance()->server_session_created(this);
}

basic_server_session::~basic_server_session()
{
    TRACE(serverLogger) << this << " socket " << m_sock;
    server::instance()->server_session_deleted(this);
}

void basic_server_session::init_session()
{
    m_event_loop->register_session(this, EPOLLOUT | EPOLLIN | EPOLLPRI | EPOLLRDHUP | EPOLLET | EPOLLERR);
}

const sockaddr_storage &basic_server_session::peer_address() const noexcept
{
    return m_peer_addr;
}

} // namespace Getodac
