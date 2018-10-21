#include <gtest/gtest.h>
#include <boost/filesystem.hpp>
#include <iostream>
#include "global.h"

struct ThrowBase
{
    int a;
    ThrowBase()
    {
        std::cout << "ThrowBase()\n";
        throw std::runtime_error{"ThrowBase"};
    }
    ~ThrowBase()
    {
        std::cout << "~ThrowBase()\n";
    }
};

struct TestConstDest
{
    TestConstDest()
    {
        std::cout << "TestConstDest()\n";
    }
    ~TestConstDest()
    {
        std::cout << "~TestConstDest()\n";
    }
};

struct TestThrowBase : public ThrowBase
{
    TestConstDest t;
    TestThrowBase()
    {
        std::cout << "TestThrowBase()\n";
    }
    ~TestThrowBase()
    {
        std::cout << "~TestThrowBase()\n";
    }
};

struct TestThrowField
{
    TestConstDest t;
    ThrowBase b;
};

int main(int argc, char **argv) {
    startGetodacServer(boost::filesystem::canonical(boost::filesystem::path(argv[0])).parent_path().append("/GETodac").string());
    ::testing::InitGoogleTest(&argc, argv);
    int res = RUN_ALL_TESTS();
    terminateGetodacServer();
    return res;
}
