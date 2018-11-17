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
#include <getodac/abstract_service_session.h>

namespace {
using namespace Getodac;
using namespace std;

    TEST(OStreamBuffer, hexLen)
    {
        EXPECT_EQ(OStreamBuffer::hexLen(0), 1);
        EXPECT_EQ(OStreamBuffer::hexLen(5), 1);
        EXPECT_EQ(OStreamBuffer::hexLen(15), 1);
        EXPECT_EQ(OStreamBuffer::hexLen(16), 2);
        EXPECT_EQ(OStreamBuffer::hexLen(125), 2);
        EXPECT_EQ(OStreamBuffer::hexLen(255), 2);
        EXPECT_EQ(OStreamBuffer::hexLen(256), 3);
        EXPECT_EQ(OStreamBuffer::hexLen(4095), 3);
        EXPECT_EQ(OStreamBuffer::hexLen(4096), 4);
        EXPECT_EQ(OStreamBuffer::hexLen(16378), 4);
        EXPECT_EQ(OStreamBuffer::hexLen(65535), 4);
        EXPECT_EQ(OStreamBuffer::hexLen(178927786), 7);
    }
}
