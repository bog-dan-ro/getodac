#include <gtest/gtest.h>
#include "global.h"

namespace {
using namespace std;


TEST(ResponseStatusError, constructor)
{
    try {
        EasyCurl curl;
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
        EasyCurl curl;
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
        EasyCurl curl;
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
        EasyCurl curl;
        EXPECT_NO_THROW(curl.setUrl("http://localhost:8080/testThowFromBody"));
        auto reply = curl.request("GET", "some data");
        EXPECT_EQ(reply.status, "400");
        EXPECT_EQ(reply.headers["BodyKey1"], "Value1");
        EXPECT_EQ(reply.headers["BodyKey2"], "Value2");
        EXPECT_EQ(reply.body, "Body too big, lose some weight");
    } catch(...) {
        EXPECT_NO_THROW(throw);
    }
}

} // namespace {
