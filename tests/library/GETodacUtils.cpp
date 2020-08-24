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
#include <dracon/utils.h>
#include <memory>

namespace {
using namespace dracon;
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
        EXPECT_THROW(fromHex('H'), std::invalid_argument);
    }

    TEST(Utils, unescape_url)
    {
        EXPECT_EQ(unescape_url("plainText"), "plainText");
        EXPECT_EQ(unescape_url("--%3D%3D+c%C3%A2nd+%229+%22+%2B+1+nu+fac+%2210%22+%3F+%3D+%21%40%23%24%25%5E%26%2A%3F%3E%3C%3A%27%5C%7C%5D%5B%60%7E+%21+%2A+%27+%28+%29+%3B+%3A+%40+%26+%3D+%2B+%24+%2C+%2F+%3F+%25+%23+%5B+%5D%3D%3D--"),
                  std::string{R"(--== când "9 " + 1 nu fac "10" ? = !@#$%^&*?><:'\|][`~ ! * ' ( ) ; : @ & = + $ , / ? % # [ ]==--)"});
        EXPECT_EQ(unescape_url("%20"), " ");
        EXPECT_THROW(unescape_url("plain%2hText"), std::invalid_argument);
        EXPECT_THROW(unescape_url("Text%2"), std::invalid_argument);
    }

    TEST(Utils, split)
    {
        string str = unescape_url("///api/v1/parents/123/children/");
        SplitVector expected = {"api", "v1", "parents", "123", "children"};
        auto splitted = split(str, '/');
        EXPECT_EQ(splitted.size(), expected.size());
        for (size_t i = 0; i < splitted.size(); ++i)
            EXPECT_EQ(expected[i], splitted[i]);
    }

    TEST(Utils, lru_cache)
    {
        using ptr = std::shared_ptr<int>;
        dracon::lru_cache<int, ptr> cache{2};
        std::vector<ptr> all{10};
        for (auto & p : all)
            p = std::make_shared<int>();
        cache.put(2, all[2]);
        EXPECT_EQ(cache.reference(2).use_count(), 2);
        EXPECT_EQ(cache.reference(2).get(), all[2].get());

        cache.put(1, all[1]);
        EXPECT_EQ(cache.reference(1).use_count(), 2);
        EXPECT_EQ(cache.reference(1).get(), all[1].get());

        // Move 2 to the top
        EXPECT_EQ(cache.reference(2).get(), all[2].get());

        cache.put(0, all[0]);
        EXPECT_EQ(cache.reference(0).use_count(), 2);
        EXPECT_EQ(cache.reference(0).get(), all[0].get());

        // 1 should be out from cache
        EXPECT_EQ(all[1].use_count(), 1);

        cache.put(0, all[3]);
        EXPECT_EQ(cache.reference(0).use_count(), 2);
        EXPECT_EQ(cache.reference(0).get(), all[3].get());
        EXPECT_EQ(all[0].use_count(), 1);

        cache.clear();
        for (size_t i = 0; i <all.size(); ++i) {
            EXPECT_EQ(all[i].use_count(), 1);
            cache.put(i % 2, all[i]);
        }

        EXPECT_EQ(cache.reference(0).use_count(), 2);
        EXPECT_EQ(cache.reference(0).get(), all[8].get());
        EXPECT_EQ(cache.reference(1).use_count(), 2);
        EXPECT_EQ(cache.reference(1).get(), all[9].get());

        for (auto it = cache.begin(); it != cache.end();)
            it = cache.erase(it);

        EXPECT_EQ(cache.size(), 0);
    }

    TEST(Utils, simple_timer)
    {
        using namespace std::chrono_literals;
        auto start = std::chrono::system_clock::now();
        std::condition_variable time_out_wait;
        std::mutex mutex;
        simple_timer st{[&]{time_out_wait.notify_one();}, 50ms};
        std::unique_lock lock(mutex);
        time_out_wait.wait(lock);
        time_out_wait.wait(lock);
        EXPECT_GE(std::chrono::system_clock::now(), (start + 100ms));
    }

    TEST(Utils, SimpleTimer_singleShot)
    {
        using namespace std::chrono_literals;
        auto start = std::chrono::system_clock::now();
        std::condition_variable timeOutWait;
        std::mutex mutex;
        simple_timer st{[&]{timeOutWait.notify_one();}, 50ms, true};
        std::unique_lock lock(mutex);
        timeOutWait.wait(lock);
        EXPECT_GE(std::chrono::system_clock::now(), (start + 50ms));
        EXPECT_EQ(timeOutWait.wait_for(lock, 100ms), std::cv_status::timeout);
    }
}
