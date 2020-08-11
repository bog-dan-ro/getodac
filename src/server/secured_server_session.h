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

#ifndef SECURED_SERVER_SESSION_H
#define SECURED_SERVER_SESSION_H

#include "server_session.h"
#include <openssl/err.h>

namespace Getodac {

class SecuredServerSession : public ServerSession
{
public:
    SecuredServerSession(SessionsEventLoop *eventLoop, int sock, const sockaddr_storage &sockAddr, uint32_t order, uint32_t epollet);
    ~SecuredServerSession() override;

    // ServerSession interface
    bool isSecuredConnection() const override { return true; }
    void verifyPeer(const std::string &caFile) override;
    X509* getPeerCertificate() const override;

    ssize_t sockRead(void *buf, size_t size) override
    {
        // make sure we start reading with no pending errors
        ERR_clear_error();
        auto sz = SSL_read(m_SSL, buf, size < INT_MAX ? static_cast<int>(size) : INT_MAX);
        if (sz <= 0) {
            int err = SSL_get_error(m_SSL, sz);
            if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
                return 0;
            } else {
                if (err == SSL_ERROR_SYSCALL || err == SSL_ERROR_SSL)
                    m_shutdown = 0;
                return -1;
            }
        }
        return sz;
    }

    ssize_t sockWrite(const void *buf, size_t size) override
    {
        // make sure we start writing with no pending errors
        ERR_clear_error();
        auto sz = SSL_write(m_SSL, buf, size < INT_MAX ? static_cast<int>(size) : INT_MAX);
        if (sz <= 0) {
            int err = SSL_get_error(m_SSL, sz);
            if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
                return 0;
            } else {
                if (err == SSL_ERROR_SYSCALL || err == SSL_ERROR_SSL)
                    m_shutdown = 0;
                return -1;
            }
        }
        return sz;
    }

    ssize_t sockWritev(const iovec *vec, int count) override;

    bool sockShutdown() override
    {
        if (m_shutdown-- && !SSL_is_init_finished(m_SSL)) {
            // Don't call SSL_shutdown() if handshake wasn't completed.
            if (SSL_shutdown(m_SSL) == 0)
                return false;
        }
        return ServerSession::sockShutdown();
    }

    static int verify_callback(int preverify_ok, X509_STORE_CTX *ctx);

    Action initSocket(YieldType &yield) override;
    void messageComplete() override;

private:
    SSL *m_SSL = nullptr;
    bool m_renegotiate = false;
    YieldType *m_ioYield = nullptr;
    uint8_t m_shutdown = 5;
};

} // namespace Getodac

#endif // SECURED_SERVER_SESSION_H
