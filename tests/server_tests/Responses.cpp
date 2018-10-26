/*
    Copyright (C) 2018, BogDan Vatra <bogdan@kde.org>

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

#include <gtest/gtest.h>
#include <EasyCurl.h>

std::string hugeData;

namespace {
using namespace std;

TEST(Responses, zero)
{
    try {
        Getodac::Test::EasyCurl curl;
        EXPECT_NO_THROW(curl.setUrl("http://localhost:8080/test0"));
        auto reply = curl.get();
        EXPECT_EQ(reply.status, "200");
        EXPECT_EQ(reply.headers["Connection"], "keep-alive");
        EXPECT_EQ(reply.headers["Content-Length"], "0");
        EXPECT_EQ(reply.headers["Keep-Alive"], "timeout=10");
        EXPECT_EQ(reply.body.empty(), true);
    } catch(...) {
        EXPECT_NO_THROW(throw);
    }
}

TEST(Responses, test100)
{
    try {
        Getodac::Test::EasyCurl curl;
        EXPECT_NO_THROW(curl.setUrl("http://localhost:8080/test100"));
        auto reply = curl.get();
        EXPECT_EQ(reply.status, "200");
        EXPECT_EQ(reply.headers["Connection"], "keep-alive");
        EXPECT_EQ(reply.headers["Content-Length"], "100");
        EXPECT_EQ(reply.headers["Keep-Alive"], "timeout=10");
        EXPECT_EQ(reply.body, "100XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX");
    } catch(...) {
        EXPECT_NO_THROW(throw);
    }
}

TEST(Responses, test50m)
{
    try {
        Getodac::Test::EasyCurl curl;
        EXPECT_NO_THROW(curl.setUrl("http://localhost:8080/test50m"));
        auto reply = curl.get();
        EXPECT_EQ(reply.status, "200");
        EXPECT_EQ(reply.headers["Connection"], "keep-alive");
        EXPECT_EQ(reply.headers["Content-Length"], "52428800");
        EXPECT_EQ(reply.headers["Keep-Alive"], "timeout=10");
        EXPECT_EQ(reply.body, hugeData);
    } catch(...) {
        EXPECT_NO_THROW(throw);
    }
}

TEST(Responses, test50m_iovec)
{
    try {
        Getodac::Test::EasyCurl curl;
        EXPECT_NO_THROW(curl.setUrl("http://localhost:8080/test50ms"));
        auto reply = curl.get();
        EXPECT_EQ(reply.status, "200");
        EXPECT_EQ(reply.headers["Connection"], "keep-alive");
        EXPECT_EQ(reply.headers["Content-Length"], "52428800");
        EXPECT_EQ(reply.headers["Keep-Alive"], "timeout=10");
        EXPECT_EQ(reply.body, hugeData);
    } catch(...) {
        EXPECT_NO_THROW(throw);
    }
}

TEST(Responses, test50mChuncked)
{
    try {
        Getodac::Test::EasyCurl curl;
        EXPECT_NO_THROW(curl.setUrl("http://localhost:8080/test50mChuncked"));
        auto reply = curl.get();
        EXPECT_EQ(reply.status, "200");
        EXPECT_EQ(reply.headers["Connection"], "keep-alive");
        EXPECT_EQ(reply.headers["Transfer-Encoding"], "chunked");
        EXPECT_EQ(reply.headers["Keep-Alive"], "timeout=10");
        EXPECT_EQ(reply.body, hugeData);
    } catch(...) {
        EXPECT_NO_THROW(throw);
    }
}

TEST(Responses, test50mChunckedAtOnce)
{
    try {
        Getodac::Test::EasyCurl curl;
        EXPECT_NO_THROW(curl.setUrl("http://localhost:8080/test50mChunckedAtOnce"));
        auto reply = curl.get();
        EXPECT_EQ(reply.status, "200");
        EXPECT_EQ(reply.headers["Connection"], "keep-alive");
        EXPECT_EQ(reply.headers["Transfer-Encoding"], "chunked");
        EXPECT_EQ(reply.headers["Keep-Alive"], "timeout=10");
        EXPECT_EQ(reply.body, hugeData);
    } catch(...) {
        EXPECT_NO_THROW(throw);
    }
}


} // namespace {
