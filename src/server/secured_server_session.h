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

#ifndef SECURED_SERVER_SESSION_H
#define SECURED_SERVER_SESSION_H

#include "server_session.h"
#include <openssl/ssl.h>
#include <openssl/err.h>

namespace Getodac {

class SecuredServerSession : public ServerSession
{
public:
    SecuredServerSession(SessionsEventLoop *eventLoop, int sock, const sockaddr_storage &sockAddr);
    ~SecuredServerSession();

    // ServerSession interface
    bool isSecuredConnection() const override { return true; }

    ssize_t read(void *buf, size_t size) override
    {
        auto sz = SSL_read(m_SSL, buf, size);
        if (sz == 0) {
            int err = SSL_get_error(m_SSL, sz);
            if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE)
                return 0;
            else
                return -1;
        }
        return sz;
    }

    ssize_t write(const void *buf, size_t size) override
    {
        auto sz = SSL_write(m_SSL, buf, size);
        if (sz == 0) {
            int err = SSL_get_error(m_SSL, sz);
            if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE)
                return 0;
            else
                return -1;
        }
        return sz;
    }

    ssize_t writev(const iovec *vec, int count) override
    {
        ssize_t written = 0;
        for (int i = 0; i < count; i++) {
            if (!vec[i].iov_len)
                continue;
            auto sz = write(vec[i].iov_base, vec[i].iov_len);
            if (sz < 0)
                break;
            written += sz;
        }
        return written;
    }

    bool shutdown() override
    {
        if (m_SSL && SSL_shutdown(m_SSL) == 0)
            return false;
        return ServerSession::shutdown();
    }

private:
    SSL *m_SSL = nullptr;
};

} // namespace Getodac

#endif // SECURED_SERVER_SESSION_H
