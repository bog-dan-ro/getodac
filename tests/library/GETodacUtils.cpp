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
#include <getodac/utils.h>

namespace {
using namespace Getodac;
using namespace std;

    TEST(Utils, fromHex)
    {
        EXPECT_EQ(fromHex('0'), 0);
        EXPECT_EQ(fromHex('5'), 5);
        EXPECT_EQ(fromHex('9'), 9);

        EXPECT_EQ(fromHex('a'), 10);
        EXPECT_EQ(fromHex('d'), 13);
        EXPECT_EQ(fromHex('f'), 15);

        EXPECT_EQ(fromHex('A'), 10);
        EXPECT_EQ(fromHex('D'), 13);
        EXPECT_EQ(fromHex('F'), 15);

        // Bad hex value, should throw an exception
        EXPECT_THROW(fromHex('H'), std::runtime_error);
    }

    TEST(Utils, unEscapeUrl)
    {
        EXPECT_EQ(unEscapeUrl("plainText"), "plainText");
        EXPECT_EQ(unEscapeUrl("--%3D%3D+c%C3%A2nd+%229+%22+%2B+1+nu+fac+%2210%22+%3F+%3D+%21%40%23%24%25%5E%26%2A%3F%3E%3C%3A%27%5C%7C%5D%5B%60%7E+%21+%2A+%27+%28+%29+%3B+%3A+%40+%26+%3D+%2B+%24+%2C+%2F+%3F+%25+%23+%5B+%5D%3D%3D--"),
                  std::string{R"(--== când "9 " + 1 nu fac "10" ? = !@#$%^&*?><:'\|][`~ ! * ' ( ) ; : @ & = + $ , / ? % # [ ]==--)"});
        EXPECT_EQ(unEscapeUrl("%20"), " ");
        EXPECT_THROW(unEscapeUrl("plain%2hText"), std::runtime_error);
        EXPECT_THROW(unEscapeUrl("Text%2"), std::runtime_error);
    }

    TEST(Utils, split)
    {
        string str = unEscapeUrl("///api/v1/parents/123/children/");
        SplitVector expected = {"api", "v1", "parents", "123", "children"};
        auto splitted = split(str, '/');
        EXPECT_EQ(splitted.size(), expected.size());
        for (size_t i = 0; i < splitted.size(); ++i)
            EXPECT_EQ(expected[i], splitted[i]);
    }
}
