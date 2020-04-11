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
#include <boost/algorithm/string/predicate.hpp>

namespace {
using namespace std;

void parseEchoData(const std::string &data, size_t &contentLength, std::unordered_map<string, string> &headers, std::string &body)
{
    contentLength = 0;
    size_t pos = 0;
    while (pos < data.size()) {
        auto p = data.find('\n', pos);
        if (p == std::string::npos)
            break;
        auto line = data.substr(pos, p - pos);
        if (boost::starts_with(line, "~~~~ ")) {
            if (boost::starts_with(line.c_str() + 5, "ContentLength: ")) {
                contentLength = std::strtoul(line.c_str() + 20, nullptr, 10);
            } else if (boost::starts_with(line.c_str() + 5, "Body:")) {
                body = data.substr(p + 1);
                break;
            }
        } else {
            auto sepPos = line.find(" : ");
            headers[line.substr(0, sepPos)] = line.substr(sepPos + 3);
        }
        pos = p + 1;
    }
}
const std::string testBodyData = R"(/*
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
 */)";
TEST(Stress, server)
{
    try {
        Getodac::Test::EasyCurl curl;
        EXPECT_NO_THROW(curl.setUrl("http://localhost:8080/echoTest"));
        EXPECT_NO_THROW(curl.setHeaders({
                                            {"Super__________________long_______________field",
                                             "with___________super________log____---------value"}
                                        }));
        auto reply = curl.post(testBodyData);
        EXPECT_EQ(reply.status, "200");
        EXPECT_EQ(reply.headers["Connection"], "keep-alive");
        EXPECT_EQ(reply.headers["Keep-Alive"], "timeout=10");
        EXPECT_EQ(reply.headers["Transfer-Encoding"], "chunked");
        size_t contentLength;
        std::unordered_map<string, string> headers;
        std::string body;
        EXPECT_NO_THROW(parseEchoData(reply.body, contentLength, headers, body));
        EXPECT_EQ(contentLength, testBodyData.size());
        EXPECT_EQ(headers["Super__________________long_______________field"],
                "with___________super________log____---------value");
        EXPECT_EQ(body, testBodyData);
    } catch(...) {
        EXPECT_NO_THROW(throw);
    }
}

} // namespace
