/*
    Copyright (C) 2022 by BogDan Vatra <bogdan@kde.org>

    Permission to use, copy, modify, and/or distribute this software for any purpose with or without fee is hereby granted.

    THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

#include <gtest/gtest.h>
#include <EasyCurl.h>

namespace {
using namespace std;

static std::string url{"http://localhost:8080/v1"};

TEST(DevicesTest, no_devices)
{
    try {
        Getodac::Test::EasyCurl curl;
        EXPECT_NO_THROW(curl.setUrl(url + "/devices"));
        auto reply = curl.get();
        EXPECT_EQ(reply.status, "200");
        EXPECT_EQ(reply.body, "[]");
    } catch(...) {
        EXPECT_NO_THROW(throw);
    }
}

TEST(DevicesTest, post_devices)
{
    try {
        Getodac::Test::EasyCurl curl;
        EXPECT_NO_THROW(curl.setUrl(url + "/devices"));
        auto reply = curl.post(R"(["dev1","dev2", "dev3"])");
        EXPECT_EQ(reply.status, "200");
        EXPECT_EQ(reply.body, "");
    } catch(...) {
        EXPECT_NO_THROW(throw);
    }
}

TEST(DevicesTest, get_devices)
{
    try {
        Getodac::Test::EasyCurl curl;
        EXPECT_NO_THROW(curl.setUrl(url + "/devices"));
        auto reply = curl.get();
        EXPECT_EQ(reply.status, "200");
        EXPECT_EQ(reply.body, R"([{"id:":0,"name":"dev1"},{"id:":1,"name":"dev2"},{"id:":2,"name":"dev3"}])");
    } catch(...) {
        EXPECT_NO_THROW(throw);
    }
}

TEST(DevicesTest, get_device)
{
    try {
        Getodac::Test::EasyCurl curl;
        EXPECT_NO_THROW(curl.setUrl(url + "/devices/1"));
        auto reply = curl.get();
        EXPECT_EQ(reply.status, "200");
        EXPECT_EQ(reply.body, R"([{"id:":1,"name":"dev2"}])");
    } catch(...) {
        EXPECT_NO_THROW(throw);
    }
}

TEST(DevicesTest, delete_device)
{
    try {
        Getodac::Test::EasyCurl curl;
        EXPECT_NO_THROW(curl.setUrl(url + "/devices/1"));
        auto reply = curl.del();
        EXPECT_EQ(reply.status, "200");
        EXPECT_EQ(reply.body, "");
    } catch(...) {
        EXPECT_NO_THROW(throw);
    }
}

TEST(DevicesTest, get_devices_after_delete)
{
    try {
        Getodac::Test::EasyCurl curl;
        EXPECT_NO_THROW(curl.setUrl(url + "/devices"));
        auto reply = curl.get();
        EXPECT_EQ(reply.status, "200");
        EXPECT_EQ(reply.body, R"([{"id:":0,"name":"dev1"},{"id:":1,"name":"dev3"}])");
    } catch(...) {
        EXPECT_NO_THROW(throw);
    }
}

TEST(DevicesTest, delete_all_devices)
{
    try {
        Getodac::Test::EasyCurl curl;
        EXPECT_NO_THROW(curl.setUrl(url + "/devices/0"));
        auto reply = curl.del();
        EXPECT_EQ(reply.status, "200");
        EXPECT_EQ(reply.body, "");
        reply = curl.del();
        EXPECT_EQ(reply.status, "200");
        EXPECT_EQ(reply.body, "");
        reply = curl.del();
        EXPECT_EQ(reply.status, "400");
        EXPECT_EQ(reply.body, "");
    } catch(...) {
        EXPECT_NO_THROW(throw);
    }
}

TEST(DevicesTest, get_invalid_device)
{
    try {
        Getodac::Test::EasyCurl curl;
        EXPECT_NO_THROW(curl.setUrl(url + "/devices/bla"));
        auto reply = curl.get();
        EXPECT_EQ(reply.status, "500");
        EXPECT_NO_THROW(curl.setUrl(url + "/devices/0"));
        reply = curl.get();
        EXPECT_EQ(reply.status, "400");
    } catch(...) {
        EXPECT_NO_THROW(throw);
    }
}


} // namespace {
