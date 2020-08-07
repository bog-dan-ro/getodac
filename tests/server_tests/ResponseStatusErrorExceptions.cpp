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

#include <chrono>
#include <gtest/gtest.h>
#include <EasyCurl.h>

#include "Utils.h"

namespace {
using namespace std;


using ResponseStatusError = testing::TestWithParam<std::string>;

TEST_P(ResponseStatusError, constructor)
{
    try {
        using clock = std::chrono::high_resolution_clock;
        auto start = clock::now();
        Getodac::Test::EasyCurl curl;
        EXPECT_NO_THROW(curl.setUrl(url(GetParam(), "/test100")));
        curl.ingnoreInvalidSslCertificate();
        auto reply = curl.get();
        EXPECT_EQ(reply.status, "200");

        EXPECT_NO_THROW(curl.setUrl("http://localhost:8080/secureOnly"));
        reply = curl.get();
        EXPECT_EQ(reply.status, "403");
        EXPECT_EQ(reply.headers["ErrorKey1"], "Value1");
        EXPECT_EQ(reply.headers["ErrorKey2"], "Value2");
        EXPECT_EQ(reply.body, "Only secured connections allowed");

        EXPECT_NO_THROW(curl.setUrl(url(GetParam(), "/test100")));
        curl.ingnoreInvalidSslCertificate();
        reply = curl.get();
        EXPECT_EQ(reply.status, "200");
        EXPECT_LE(std::chrono::duration_cast<std::chrono::milliseconds>(clock::now() - start).count(),  duration(GetParam(), 10));
    } catch(...) {
        EXPECT_NO_THROW(throw);
    }
}

TEST_P(ResponseStatusError, fromHeaderValue)
{
    try {
        Getodac::Test::EasyCurl curl;
        EXPECT_NO_THROW(curl.setUrl(url(GetParam(), "/testThowFromHeaderFieldValue")));
        curl.ingnoreInvalidSslCertificate();
        auto reply = curl.get();
        EXPECT_EQ(reply.status, "400");
        EXPECT_EQ(reply.headers["ErrorKey1"], "Value1");
        EXPECT_EQ(reply.headers["ErrorKey2"], "Value2");
        EXPECT_EQ(reply.body, "Too many headers");
    } catch(...) {
        EXPECT_NO_THROW(throw);
    }
}

TEST_P(ResponseStatusError, fromHeadersComplete)
{
    try {
        Getodac::Test::EasyCurl curl;
        EXPECT_NO_THROW(curl.setUrl(url(GetParam(), "/testThowFromHeadersComplete")));
        curl.ingnoreInvalidSslCertificate();
        auto reply = curl.get();
        EXPECT_EQ(reply.status, "401");
        EXPECT_EQ(reply.headers["WWW-Authenticate"], "Basic realm=\"Restricted Area\"");
        EXPECT_EQ(reply.headers["ErrorKey2"], "Value2");
        EXPECT_EQ(reply.body, "What are you doing here?");
    } catch(...) {
        EXPECT_NO_THROW(throw);
    }
}

TEST_P(ResponseStatusError, fromRequestComplete)
{
    try {
        Getodac::Test::EasyCurl curl;
        EXPECT_NO_THROW(curl.setUrl(url(GetParam(), "/testThowFromRequestComplete")));
        curl.ingnoreInvalidSslCertificate();
        auto reply = curl.get();
        EXPECT_EQ(reply.status, "412");
        EXPECT_EQ(reply.body.size(), 0);
    } catch(...) {
        EXPECT_NO_THROW(throw);
    }
}

TEST_P(ResponseStatusError, expectation)
{
    try {
        Getodac::Test::EasyCurl curl;
        EXPECT_NO_THROW(curl.setUrl(url(GetParam(), "/testExpectation")));
        curl.ingnoreInvalidSslCertificate();
        auto reply = curl.post("some data");
        EXPECT_EQ(reply.status, "417");
        EXPECT_EQ(reply.body.size(), 0);
    } catch(...) {
        EXPECT_NO_THROW(throw);
    }
}

TEST_P(ResponseStatusError, fromBody)
{
    try {
        Getodac::Test::EasyCurl curl;
        EXPECT_NO_THROW(curl.setUrl(url(GetParam(), "/testThowFromBody")));
        curl.ingnoreInvalidSslCertificate();
        auto reply = curl.post("some data");
        EXPECT_EQ(reply.status, "400");
        EXPECT_EQ(reply.headers["BodyKey1"], "Value1");
        EXPECT_EQ(reply.headers["BodyKey2"], "Value2");
        EXPECT_EQ(reply.body, "Body too big, lose some weight");
    } catch(...) {
        EXPECT_NO_THROW(throw);
    }
}

TEST_P(ResponseStatusError, fromWriteResponse)
{
    try {
        Getodac::Test::EasyCurl curl;
        EXPECT_NO_THROW(curl.setUrl(url(GetParam(), "/testThowFromWriteResponse")));
        curl.ingnoreInvalidSslCertificate();
        auto reply = curl.get();
        EXPECT_EQ(reply.status, "409");
        EXPECT_EQ(reply.headers["WriteRes1"], "Value1");
        EXPECT_EQ(reply.headers["WriteRes2"], "Value2");
        EXPECT_EQ(reply.body, "Throw from WriteResponse");
    } catch(...) {
        EXPECT_NO_THROW(throw);
    }
}

TEST_P(ResponseStatusError, fromWriteResponseStd)
{
    try {
        Getodac::Test::EasyCurl curl;
        using clock = std::chrono::high_resolution_clock;
        auto start = clock::now();
        EXPECT_NO_THROW(curl.setUrl(url(GetParam(), "/test100")));
        curl.ingnoreInvalidSslCertificate();
        auto reply = curl.get();
        EXPECT_EQ(reply.status, "200");

        EXPECT_NO_THROW(curl.setUrl(url(GetParam(), "/testThowFromWriteResponseStd")));
        curl.ingnoreInvalidSslCertificate();
        reply = curl.get();
        EXPECT_EQ(reply.status, "500");
        EXPECT_EQ(reply.body, "Throw from WriteResponseStd");

        EXPECT_NO_THROW(curl.setUrl(url(GetParam(), "/test100")));
        curl.ingnoreInvalidSslCertificate();
        reply = curl.get();
        EXPECT_EQ(reply.status, "200");
        EXPECT_LE(std::chrono::duration_cast<std::chrono::milliseconds>(clock::now() - start).count(),  duration(GetParam(), 10));
    } catch(...) {
        EXPECT_NO_THROW(throw);
    }
}

TEST_P(ResponseStatusError, fromWriteResponseAfterWrite)
{
    try {
        Getodac::Test::EasyCurl curl;
        using clock = std::chrono::high_resolution_clock;
        auto start = clock::now();
        EXPECT_NO_THROW(curl.setUrl(url(GetParam(), "/test100")));
        curl.ingnoreInvalidSslCertificate();
        auto reply = curl.get();
        EXPECT_EQ(reply.status, "200");

        EXPECT_NO_THROW(curl.setUrl(url(GetParam(), "/testThowFromWriteResponseAfterWrite")));
        curl.ingnoreInvalidSslCertificate();
        EXPECT_THROW(curl.get(), std::runtime_error);

        EXPECT_NO_THROW(curl.setUrl(url(GetParam(), "/test100")));
        curl.ingnoreInvalidSslCertificate();
        reply = curl.get();
        EXPECT_EQ(reply.status, "200");
        EXPECT_LE(std::chrono::duration_cast<std::chrono::milliseconds>(clock::now() - start).count(), duration(GetParam(), 10));
    } catch(...) {
        EXPECT_NO_THROW(throw);
    }
}

TEST_P(ResponseStatusError, afterWakeup)
{
    try {
        Getodac::Test::EasyCurl curl;
        using clock = std::chrono::high_resolution_clock;
        auto start = clock::now();
        EXPECT_NO_THROW(curl.setUrl(url(GetParam(), "/test100")));
        curl.ingnoreInvalidSslCertificate();
        auto reply = curl.get();
        EXPECT_EQ(reply.status, "200");

        EXPECT_NO_THROW(curl.setUrl(url(GetParam(), "/testThrowAfterWakeup")));
        curl.ingnoreInvalidSslCertificate();
        reply = curl.get();
        EXPECT_EQ(reply.status, "404");

        EXPECT_NO_THROW(curl.setUrl(url(GetParam(), "/test100")));
        curl.ingnoreInvalidSslCertificate();
        reply = curl.get();
        EXPECT_EQ(reply.status, "200");
        EXPECT_LE(std::chrono::duration_cast<std::chrono::milliseconds>(clock::now() - start).count(), duration(GetParam(), 150));
    } catch(...) {
        EXPECT_NO_THROW(throw);
    }
}

INSTANTIATE_TEST_SUITE_P(ResponseStatusError, ResponseStatusError, testing::Values("http", "https"));

} // namespace {
