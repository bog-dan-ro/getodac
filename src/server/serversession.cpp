/*
    Copyright (C) 2021, BogDan Vatra <bogdan@kde.org>

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

#include "serversession.h"

namespace Getodac {

BasicServerSession::BasicServerSession(Getodac::SessionsEventLoop *event_loop, int sock, const sockaddr_storage &sock_addr, uint32_t order)
    : m_sock(sock)
    , m_order(order)
    , m_peerAddr(sock_addr)
    , m_eventLoop(event_loop)
{
    Server::instance().serverSessionCreated(this);
}

BasicServerSession::~BasicServerSession()
{
    TRACE(server_logger) << this << " socket " << m_sock;
    Server::instance().serverSessionDeleted(this);
}

void BasicServerSession::initSession()
{
    m_eventLoop->registerSession(this, EPOLLOUT | EPOLLIN | EPOLLPRI | EPOLLRDHUP | EPOLLET | EPOLLERR);
}

const sockaddr_storage &BasicServerSession::peerAddress() const noexcept
{
    return m_peerAddr;
}

} // namespace Getodac
