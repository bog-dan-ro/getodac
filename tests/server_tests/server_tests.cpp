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
#include <boost/filesystem.hpp>
#include <iostream>

#include <GETodacServer.h>

extern std::string hugeData;

int main(int argc, char **argv) {
    Getodac::Test::startServer(boost::filesystem::canonical(boost::filesystem::path(argv[0])).parent_path().append("/GETodac").string());
    for (int i = 0; i < 50 * 1024 * 1024; ++i)
        hugeData += char(33 + (i % 93));
    ::testing::InitGoogleTest(&argc, argv);
    int res = RUN_ALL_TESTS();
    Getodac::Test::terminateServer();
    return res;
}
