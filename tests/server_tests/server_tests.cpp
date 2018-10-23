#include <gtest/gtest.h>
#include <boost/filesystem.hpp>
#include <iostream>
#include "global.h"

int main(int argc, char **argv) {
    startGetodacServer(boost::filesystem::canonical(boost::filesystem::path(argv[0])).parent_path().append("/GETodac").string());
    ::testing::InitGoogleTest(&argc, argv);
    int res = RUN_ALL_TESTS();
    terminateGetodacServer();
    return res;
}
