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
#include <getodac/restful.h>

namespace {
    using namespace std;
    using namespace Getodac;
    class TestRRType : public RESTfulResource<int, ParsedUrl>
    {
    public:
        TestRRType(const std::string &resource = {})
            : RESTfulResource(resource)
        {}

        const std::string &allMethods() const
        {
            return d->allMethods;
        }

        const std::string &resource() const
        {
            return d->resource;
        }

        const std::unordered_map<std::string, RESTfulResourceMethodCreator<int, ParsedUrl>> &methods() const
        {
            return d->methods;
        }

        const vector<RESTfulResource<int, ParsedUrl>> &subResources() const
        {
            return d->subResources;
        }

    };

    TEST(RESTfulResource, addSubResource)
    {
        TestRRType rootResful{"/api/v1/"};
        TestRRType simpleSubresource{"parents"};
        rootResful.addSubResource(simpleSubresource);
        EXPECT_EQ(rootResful.subResources().size(), 1);
        rootResful.addSubResource(simpleSubresource);
        EXPECT_EQ(rootResful.subResources().size(), 2);

        TestRRType simplePlaceholder{};
        rootResful.addSubResource(simplePlaceholder);
        EXPECT_EQ(rootResful.subResources().size(), 3);

        // There can be only one subresource placeholder
        EXPECT_THROW(rootResful.addSubResource(simplePlaceholder), std::runtime_error);

        // add another subresource after adding a placeholder
        rootResful.addSubResource(simpleSubresource);
        EXPECT_EQ(rootResful.subResources().size(), 4);

        // the placeholder will be moved to the back
        EXPECT_EQ(rootResful.subResources().back(), simplePlaceholder);

        // the resource should be last but one
        EXPECT_EQ(*(rootResful.subResources().rbegin() + 1), simpleSubresource);
    }

    TEST(RESTfulResource, addMethodCreator)
    {
        TestRRType rootResful{"/api/v1/"};
        rootResful.addMethodCreator("GET", [](ParsedUrl, ParsedUrl) -> int{ return 0; });
        EXPECT_EQ(rootResful.methods().size(), 1);
        EXPECT_EQ(rootResful.allMethods(), "GET");
        EXPECT_EQ(rootResful.methods().at("GET")({}, {}), 0);

        rootResful.addMethodCreator("GET", [](ParsedUrl, ParsedUrl) -> int{ return 1; });
        EXPECT_EQ(rootResful.methods().size(), 1);
        EXPECT_EQ(rootResful.allMethods(), "GET");
        EXPECT_EQ(rootResful.methods().at("GET")({}, {}), 1);

        rootResful.addMethodCreator("POST", [](ParsedUrl, ParsedUrl) -> int{ return 2; });
        EXPECT_EQ(rootResful.methods().size(), 2);
        EXPECT_EQ(rootResful.allMethods(), "GET, POST");
        EXPECT_EQ(rootResful.methods().at("POST")({}, {}), 2);

        // OPTIONS method should be ignored
        rootResful.addMethodCreator("OPTIONS", [](ParsedUrl, ParsedUrl) -> int{ return 3; });
        EXPECT_EQ(rootResful.methods().size(), 3);
        EXPECT_EQ(rootResful.allMethods(), "GET, POST");
        EXPECT_EQ(rootResful.methods().at("OPTIONS")({}, {}), 3);
    }

    TEST(RESTfulResource, create)
    {
        map<string, int> resourcesVerbs = {{"GET", 1}, {"POST", 2}, {"OPTIONS", 12}};
        map<string, int> placeholdersVerbs = {{"GET", 1}, {"PUT", 3}, {"DELETE", 4}, {"OPTIONS", 134}};
        ParsedUrl expected {
            {// Resources
                { // first resource pair
                    {"parents"}, {}
                }
            },
            { // QueryStrings
                {
                    {"key1"}, {"value1"}
                },
                {
                    {"key2"}, {"value2"}
                }
            }
        };

        auto createResource = [&](string resource) {
            TestRRType res{std::move(resource)};
            for (auto kv : resourcesVerbs) {
                auto second = kv.second;
                res.addMethodCreator(std::move(kv.first), [second](ParsedUrl parsedUrl, ParsedUrl expectedParsedUrl) -> int {
                        expectedParsedUrl.allButOPTIONSNodeMethods = "GET, POST";
                        EXPECT_EQ(parsedUrl, expectedParsedUrl);
                        return second;
                    });
            }
            return res ;
        };

        auto createPlaceHolder = [&]() {
            TestRRType res{};
            for (auto kv : placeholdersVerbs) {
                auto second = kv.second;
                res.addMethodCreator(kv.first, [second](ParsedUrl parsedUrl, ParsedUrl expectedParsedUrl) -> int {
                        expectedParsedUrl.allButOPTIONSNodeMethods = "DELETE, GET, PUT";
                        EXPECT_EQ(parsedUrl, expectedParsedUrl);
                        return second;
                    });
            }
            return res;
        };
        TestRRType rootResful{"/api/v1/"};
//      test the follwing scenarious:
//      /api/v1/parents?key1=value1&key2=value2
//              GET
//              POST
//              OPTIONS
        TestRRType parentsResource = createResource("parents");
        rootResful.addSubResource(parentsResource);

//      /api/v1/parents/{parentId}?key1=value1&key2=value2
//              GET
//              PUT
//              DELETE
//              OPTIONS
        TestRRType parentsPlaceholder = createPlaceHolder();
        parentsResource.addSubResource(parentsPlaceholder);

//      /api/v1/parents/{parentId}/children?key1=value1&key2=value2
//              GET
//              POST
//              OPTIONS
        TestRRType childrenResource = createResource("children");
        parentsPlaceholder.addSubResource(childrenResource);

//      /api/v1/parents/{parentId}/children/{childrenId}?key1=value1&key2=value2
//              GET
//              PUT
//              DELETE
//              OPTIONS
        TestRRType childrenPlaceHolder = createPlaceHolder();
        childrenResource.addSubResource(childrenPlaceHolder);

//      /api/v1/parents/{parentId}/children/statistics?key1=value1&key2=value2
//              GET
//              POST
//              OPTIONS
        TestRRType childrenStatisticsResource = createResource("statistics");
        childrenResource.addSubResource(childrenStatisticsResource);

//      /api/v1/parents/{parentId}/children/statistics/{statsId}?key1=value1&key2=value2
//              GET
//              POST
//              OPTIONS
        TestRRType childrenStatisticsPlaceholder = createPlaceHolder();
        childrenStatisticsResource.addSubResource(childrenStatisticsPlaceholder);


        string url{"/api/v1/parents?key1=value1&key2=value2"};
        for (auto kv : resourcesVerbs)
            EXPECT_EQ(rootResful.create(url, kv.first, expected), kv.second);

        expected.resources.back().second="{parentId}";
        url = "/api/v1/parents/{parentId}?key1=value1&key2=value2";
        for (auto kv : placeholdersVerbs)
            EXPECT_EQ(rootResful.create(url, kv.first, expected), kv.second);

        expected.resources.push_back({{"children"}, {}});
        url = "/api/v1/parents/{parentId}/children?key1=value1&key2=value2";
        for (auto kv : resourcesVerbs)
            EXPECT_EQ(rootResful.create(url, kv.first, expected), kv.second);

        expected.resources.back().second="{childrenId}";
        url = "/api/v1/parents/{parentId}/children/{childrenId}?key1=value1&key2=value2";
        for (auto kv : placeholdersVerbs)
            EXPECT_EQ(rootResful.create(url, kv.first, expected), kv.second);

        expected.resources.back().second.clear();
        expected.resources.push_back({{"statistics"}, {}});
        url = "/api/v1/parents/{parentId}/children/statistics?key1=value1&key2=value2";
        for (auto kv : resourcesVerbs)
            EXPECT_EQ(rootResful.create(url, kv.first, expected), kv.second);

        expected.resources.back().second="{statsId}";
        url = "/api/v1/parents/{parentId}/children/statistics/{statsId}?key1=value1&key2=value2";
        for (auto kv : placeholdersVerbs)
            EXPECT_EQ(rootResful.create(url, kv.first, expected), kv.second);
    }
}
