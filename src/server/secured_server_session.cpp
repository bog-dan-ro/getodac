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

#include "secured_server_session.h"
#include "server.h"

#include <sys/epoll.h>

namespace Getodac {

SecuredServerSession::SecuredServerSession(SessionsEventLoop *eventLoop, int sock, const sockaddr_storage &sockAddr,
                                           uint32_t order, uint32_t epollet)
    : ServerSession(eventLoop, sock, sockAddr, order, epollet)
{
    if (!(m_SSL = SSL_new(Server::instance()->sslContext())))
        throw std::runtime_error(ERR_error_string(ERR_get_error(), nullptr));

    if (!SSL_set_fd(m_SSL, sock))
        throw std::runtime_error(ERR_error_string(SSL_get_error(m_SSL, 0), nullptr));

    m_eventLoop->updateSession(this, EPOLLIN | EPOLLOUT | EPOLLPRI | EPOLLRDHUP | m_epollet | EPOLLERR);
}

SecuredServerSession::~SecuredServerSession()
{
    try {
        SSL_free(m_SSL);
    } catch (...) {}
}

void SecuredServerSession::verifyPeer(const std::string &caFile)
{
    if (!caFile.empty()) {
        auto certs = SSL_load_client_CA_file(caFile.c_str());
        if (!certs)
            throw std::runtime_error(ERR_error_string(ERR_get_error(), nullptr));
        SSL_set_client_CA_list(m_SSL, certs);
    }
    SSL_set_verify(m_SSL, SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT | SSL_VERIFY_CLIENT_ONCE, &verify_callback);
    SSL_set_verify_depth(m_SSL, 10);
    m_renegotiate = true;
}

X509 *SecuredServerSession::getPeerCertificate() const
{
    return SSL_get_peer_certificate(m_SSL);
}

int SecuredServerSession::verify_callback(int preverify_ok, X509_STORE_CTX *ctx)
{
    (void) ctx;
    return preverify_ok;
}

AbstractServerSession::Action SecuredServerSession::initSocket(YieldType &yield)
{
    m_ioYield = &yield;
    int ret;
    while ((ret = SSL_accept(m_SSL)) != 1) {
        int err = SSL_get_error(m_SSL, ret);
        if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
            yield();
            continue;
        } else {
            m_shutdown = 0;
            return Action::Quit;
        }
    }
    return Action::Continue;
}

void SecuredServerSession::messageComplete()
{
    if (!m_renegotiate)
        return;

    m_renegotiate =  false;

    SSL_set_options(m_SSL, SSL_OP_NO_SESSION_RESUMPTION_ON_RENEGOTIATION);
    SSL_renegotiate(m_SSL);
    int ret;
    int step = 0;
    while ( (ret = SSL_do_handshake(m_SSL)) != 1) {
        if (ret <=  0) {
            auto err = SSL_get_error(m_SSL, ret);
            if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
                if (++step > 5) {
                    (*m_ioYield)();
                    step = 0;
                }
                continue;
            } else {
                if (err == SSL_ERROR_SYSCALL || err == SSL_ERROR_SSL) {
                    m_shutdown = 0;
                    setTimeout(1ms);
                }
            }
        }
        return;
    }

    step = 0;
    while ( (ret = SSL_do_handshake(m_SSL)) != 1) {
        if (ret <= 0) {
            auto err = SSL_get_error(m_SSL, ret);
            if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
                if (++step > 5) {
                    (*m_ioYield)();
                    step = 0;
                }
                continue;
            } else {
                if (err == SSL_ERROR_SYSCALL || err == SSL_ERROR_SSL) {
                    m_shutdown = 0;
                    setTimeout(1ms);
                }
            }
        }
        break;
    }
}

} // namespace Getodac
