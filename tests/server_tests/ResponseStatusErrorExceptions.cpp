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

#include <gtest/gtest.h>
#include <EasyCurl.h>

namespace {
using namespace std;


TEST(ResponseStatusError, constructor)
{
    try {
        Getodac::Test::EasyCurl curl;
        EXPECT_NO_THROW(curl.setUrl("http://localhost:8080/secureOnly"));
        auto reply = curl.get();
        EXPECT_EQ(reply.status, "400");
        EXPECT_EQ(reply.headers["ErrorKey1"], "Value1");
        EXPECT_EQ(reply.headers["ErrorKey2"], "Value2");
        EXPECT_EQ(reply.body, "Only secured connections allowed");
    } catch(...) {
        EXPECT_NO_THROW(throw);
    }
}

TEST(ResponseStatusError, fromHeaderValue)
{
    try {
        Getodac::Test::EasyCurl curl;
        EXPECT_NO_THROW(curl.setUrl("http://localhost:8080/testThowFromHeaderFieldValue"));
        auto reply = curl.get();
        EXPECT_EQ(reply.status, "400");
        EXPECT_EQ(reply.headers["ErrorKey1"], "Value1");
        EXPECT_EQ(reply.headers["ErrorKey2"], "Value2");
        EXPECT_EQ(reply.body, "Too many headers");
    } catch(...) {
        EXPECT_NO_THROW(throw);
    }
}

TEST(ResponseStatusError, fromHeadersComplete)
{
    try {
        Getodac::Test::EasyCurl curl;
        EXPECT_NO_THROW(curl.setUrl("http://localhost:8080/testThowFromHeadersComplete"));
        auto reply = curl.get();
        EXPECT_EQ(reply.status, "401");
        EXPECT_EQ(reply.headers["WWW-Authenticate"], "Basic realm=\"Restricted Area\"");
        EXPECT_EQ(reply.headers["ErrorKey2"], "Value2");
        EXPECT_EQ(reply.body, "What are you doing here?");
    } catch(...) {
        EXPECT_NO_THROW(throw);
    }
}

TEST(ResponseStatusError, fromBody)
{
    try {
        Getodac::Test::EasyCurl curl;
        EXPECT_NO_THROW(curl.setUrl("http://localhost:8080/testThowFromBody"));
        auto reply = curl.post("some data");
        EXPECT_EQ(reply.status, "417");
        EXPECT_EQ(reply.body.size(), 0);
    } catch(...) {
        EXPECT_NO_THROW(throw);
    }
}

} // namespace {
