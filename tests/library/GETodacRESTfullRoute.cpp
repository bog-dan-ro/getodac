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
#include <getodac/restful.h>
#include <optional>

namespace {
    using namespace std;
    using namespace Getodac;

    class TestRouter : public RESTfulRouter<std::optional<int>, int>
    {
    public:
        TestRouter(const std::string &baseUrl = {})
            : RESTfulRouter(baseUrl)
        {}

        const auto &baseUrl() const { return m_baseUrl; }
        const auto &routes() const { return m_routes; }
    };

    TEST(RESTfulRoute, RESTfulRoute)
    {
        {
            TestRouter testNoBase{};
            EXPECT_EQ(testNoBase.routes().size(), 0);
            EXPECT_EQ(testNoBase.baseUrl().size(), 0);
        }
        {
            TestRouter testNoBase{"///"};
            EXPECT_EQ(testNoBase.routes().size(), 0);
            EXPECT_EQ(testNoBase.baseUrl().size(), 0);
        }
        {
            TestRouter testBase{"///a/b//c///"};
            EXPECT_EQ(testBase.routes().size(), 0);
            EXPECT_EQ(testBase.baseUrl().size(), 3);
            EXPECT_EQ(testBase.baseUrl().at(0), "a");
            EXPECT_EQ(testBase.baseUrl().at(1), "b");
            EXPECT_EQ(testBase.baseUrl().at(2), "c");
        }
    }


    TEST(RESTfulRoute, createRoute)
    {
        TestRouter router{};
        auto route = router.createRoute("/parents");
        EXPECT_EQ(router.routes().size(), 1);
        route->addMethodHandler("OPTIONS", [](const ParsedRoute &parsedRoute, int a) -> std::optional<int> {
            EXPECT_EQ(parsedRoute.capturedResources.size(), 0);
            EXPECT_EQ(parsedRoute.queryStrings.size(), 0);
            EXPECT_EQ(parsedRoute.allButOPTIONSNodeMethods.size(), 0);
            return ++a;
        });
        auto route2 = router.createRoute("/parents");
        EXPECT_EQ(router.routes().size(), 1);
        EXPECT_EQ(route, route2);
        auto parentRoute = router.createRoute("/parents/{parent}");
        EXPECT_EQ(router.routes().size(), 2);
        auto parentRoute2 = router.createRoute("/parents/{parent}");
        EXPECT_EQ(router.routes().size(), 2);
        EXPECT_EQ(parentRoute, parentRoute2);
    }

    TEST(RESTfulRoute, createHandler)
    {
        TestRouter router{};
        // -------------------------------------------------------- //
        auto parentsRoute = router.createRoute("/parents");
        EXPECT_EQ(router.routes().size(), 1);
        parentsRoute->addMethodHandler("OPTIONS", [](const ParsedRoute &parsedRoute, int a) -> std::optional<int> {
            EXPECT_EQ(parsedRoute.capturedResources.size(), 0);
            EXPECT_EQ(parsedRoute.queryStrings.size(), 0);
            EXPECT_EQ(parsedRoute.allButOPTIONSNodeMethods, "GET, DELETE, POST");
            return a + 1;
        });
        parentsRoute->addMethodHandler("GET", [](const ParsedRoute &parsedRoute, int a) -> std::optional<int> {
            EXPECT_EQ(parsedRoute.capturedResources.size(), 0);
            EXPECT_EQ(parsedRoute.queryStrings.size(), 0);
            EXPECT_EQ(parsedRoute.allButOPTIONSNodeMethods, "GET, DELETE, POST");
            return a + 2;
        });
        parentsRoute->addMethodHandler("DELETE", [](const ParsedRoute &parsedRoute, int a) -> std::optional<int> {
            EXPECT_EQ(parsedRoute.capturedResources.size(), 0);
            EXPECT_EQ(parsedRoute.queryStrings.size(), 0);
            EXPECT_EQ(parsedRoute.allButOPTIONSNodeMethods, "GET, DELETE, POST");
            return a + 3;
        });
        parentsRoute->addMethodHandler("POST", [](const ParsedRoute &parsedRoute, int a) -> std::optional<int> {
            EXPECT_EQ(parsedRoute.capturedResources.size(), 0);
            EXPECT_EQ(parsedRoute.queryStrings.size(), 0);
            EXPECT_EQ(parsedRoute.allButOPTIONSNodeMethods, "GET, DELETE, POST");
            return a + 4;
        });

        // -------------------------------------------------------- //
        auto parentRoute = router.createRoute("/parents/{parent}");
        parentRoute->addMethodHandler("OPTIONS", [](const ParsedRoute &parsedRoute, int a) -> std::optional<int> {
            EXPECT_EQ(parsedRoute.capturedResources.size(), 1);
            EXPECT_EQ(parsedRoute.capturedResources.at("parent"), "1234");
            EXPECT_EQ(parsedRoute.queryStrings.size(), 0);
            EXPECT_EQ(parsedRoute.allButOPTIONSNodeMethods, "GET, DELETE, PUT, PATCH");
            return a + 10;
        });
        parentRoute->addMethodHandler("GET", [](const ParsedRoute &parsedRoute, int a) -> std::optional<int> {
            EXPECT_EQ(parsedRoute.capturedResources.size(), 1);
            EXPECT_EQ(parsedRoute.capturedResources.at("parent"), "2345");
            EXPECT_EQ(parsedRoute.queryStrings.size(), 0);
            EXPECT_EQ(parsedRoute.allButOPTIONSNodeMethods, "GET, DELETE, PUT, PATCH");
            return a + 20;
        });
        parentRoute->addMethodHandler("DELETE", [](const ParsedRoute &parsedRoute, int a) -> std::optional<int> {
            EXPECT_EQ(parsedRoute.capturedResources.size(), 1);
            EXPECT_EQ(parsedRoute.capturedResources.at("parent"), "3456");
            EXPECT_EQ(parsedRoute.queryStrings.size(), 0);
            EXPECT_EQ(parsedRoute.allButOPTIONSNodeMethods, "GET, DELETE, PUT, PATCH");
            return a + 30;
        });
        parentRoute->addMethodHandler("PUT", [](const ParsedRoute &parsedRoute, int a) -> std::optional<int> {
            EXPECT_EQ(parsedRoute.capturedResources.size(), 1);
            EXPECT_EQ(parsedRoute.capturedResources.at("parent"), "4567");
            EXPECT_EQ(parsedRoute.queryStrings.size(), 0);
            EXPECT_EQ(parsedRoute.allButOPTIONSNodeMethods, "GET, DELETE, PUT, PATCH");
            return a + 40;
        });
        parentRoute->addMethodHandler("PATCH", [](const ParsedRoute &parsedRoute, int a) -> std::optional<int> {
            EXPECT_EQ(parsedRoute.capturedResources.size(), 1);
            EXPECT_EQ(parsedRoute.capturedResources.at("parent"), "5678");
            EXPECT_EQ(parsedRoute.queryStrings.size(), 0);
            EXPECT_EQ(parsedRoute.allButOPTIONSNodeMethods, "GET, DELETE, PUT, PATCH");
            return a + 50;
        });

        // -------------------------------------------------------- //
        auto children = router.createRoute("/parents/{parent}/children");
        children->addMethodHandler("GET", [](const ParsedRoute &parsedRoute, int a) -> std::optional<int> {
            EXPECT_EQ(parsedRoute.capturedResources.size(), 1);
            EXPECT_EQ(parsedRoute.capturedResources.at("parent"), "615243");
            EXPECT_EQ(parsedRoute.queryStrings.size(), 1);
            EXPECT_EQ(parsedRoute.queryStrings.at(0).first, "key1");
            EXPECT_EQ(parsedRoute.queryStrings.at(0).second, "value1");
            EXPECT_EQ(parsedRoute.allButOPTIONSNodeMethods, "GET, DELETE, POST");
            return a + 200;
        });
        children->addMethodHandler("DELETE", [](const ParsedRoute &parsedRoute, int a) -> std::optional<int> {
            EXPECT_EQ(parsedRoute.capturedResources.size(), 1);
            EXPECT_EQ(parsedRoute.capturedResources.at("parent"), "273645");
            EXPECT_EQ(parsedRoute.queryStrings.size(), 2);
            EXPECT_EQ(parsedRoute.queryStrings.at(0).first, "key2");
            EXPECT_EQ(parsedRoute.queryStrings.at(0).second, "value2");
            EXPECT_EQ(parsedRoute.queryStrings.at(1).first, "key1");
            EXPECT_EQ(parsedRoute.queryStrings.at(1).second, "value1");
            EXPECT_EQ(parsedRoute.allButOPTIONSNodeMethods, "GET, DELETE, POST");
            return a + 300;
        });
        children->addMethodHandler("POST", [](const ParsedRoute &parsedRoute, int a) -> std::optional<int> {
            EXPECT_EQ(parsedRoute.capturedResources.size(), 1);
            EXPECT_EQ(parsedRoute.capturedResources.at("parent"), "837465");
            EXPECT_EQ(parsedRoute.queryStrings.size(), 3);
            EXPECT_EQ(parsedRoute.queryStrings.at(0).first, "key1");
            EXPECT_EQ(parsedRoute.queryStrings.at(0).second, "value1");
            EXPECT_EQ(parsedRoute.queryStrings.at(1).first, "key2");
            EXPECT_EQ(parsedRoute.queryStrings.at(1).second, "value1");
            EXPECT_EQ(parsedRoute.queryStrings.at(2).first, "q");
            EXPECT_EQ(parsedRoute.queryStrings.at(2).second, "search term");
            EXPECT_EQ(parsedRoute.allButOPTIONSNodeMethods, "GET, DELETE, POST");
            return a + 400;
        });
        children->addMethodHandler("OPTIONS", [](const ParsedRoute &parsedRoute, int a) -> std::optional<int> {
            EXPECT_EQ(parsedRoute.capturedResources.size(), 1);
            EXPECT_EQ(parsedRoute.capturedResources.at("parent"), "495867");
            EXPECT_EQ(parsedRoute.queryStrings.size(), 3);
            EXPECT_EQ(parsedRoute.queryStrings.at(0).first, "key1");
            EXPECT_EQ(parsedRoute.queryStrings.at(0).second, "value1");
            EXPECT_EQ(parsedRoute.queryStrings.at(1).first, "key1");
            EXPECT_EQ(parsedRoute.queryStrings.at(1).second, "value2");
            EXPECT_EQ(parsedRoute.queryStrings.at(2).first, "key3");
            EXPECT_EQ(parsedRoute.queryStrings.at(2).second, "value 3");
            EXPECT_EQ(parsedRoute.allButOPTIONSNodeMethods, "GET, DELETE, POST");
            return a + 100;
        });

        auto complex = router.createRoute("/parents/{mother}/{father}/children/{name}/{age}/{height}");
        complex->addMethodHandler("GET", [](const ParsedRoute &parsedRoute, int a) -> std::optional<int> {
            EXPECT_EQ(parsedRoute.capturedResources.size(), 5);
            EXPECT_EQ(parsedRoute.capturedResources.at("mother"), "Anna");
            EXPECT_EQ(parsedRoute.capturedResources.at("father"), "George");
            EXPECT_EQ(parsedRoute.capturedResources.at("name"), "Jonny");
            EXPECT_EQ(parsedRoute.capturedResources.at("age"), "14");
            EXPECT_EQ(parsedRoute.capturedResources.at("height"), "165");
            EXPECT_EQ(parsedRoute.queryStrings.size(), 3);
            EXPECT_EQ(parsedRoute.queryStrings.at(0).first, "key1");
            EXPECT_EQ(parsedRoute.queryStrings.at(0).second, "value1");
            EXPECT_EQ(parsedRoute.queryStrings.at(1).first, "key2");
            EXPECT_EQ(parsedRoute.queryStrings.at(1).second, "value2");
            EXPECT_EQ(parsedRoute.queryStrings.at(2).first, "key3");
            EXPECT_EQ(parsedRoute.queryStrings.at(2).second, "value3");
            EXPECT_EQ(parsedRoute.allButOPTIONSNodeMethods, "GET");
            return a + 1000;
        });

        // -------------------------------------------------------- //
        EXPECT_EQ(2, router.createHandler("/parents", "OPTIONS", 1));
        EXPECT_EQ(3, router.createHandler("/parents", "GET", 1));
        EXPECT_EQ(5, router.createHandler("/parents", "POST", 1));
        EXPECT_EQ(4, router.createHandler("/parents", "DELETE", 1));

        // There is no PUT method for /parents
        EXPECT_EQ(std::nullopt, router.createHandler("/parents", "PUT", 1));

        // Replace old method
        parentsRoute->addMethodHandler("OPTIONS", [](const ParsedRoute &parsedRoute, int a) -> std::optional<int> {
            EXPECT_EQ(parsedRoute.capturedResources.size(), 0);
            EXPECT_EQ(parsedRoute.queryStrings.size(), 0);
            EXPECT_EQ(parsedRoute.allButOPTIONSNodeMethods, "GET, DELETE, POST");
            return a - 1;
        });
        EXPECT_EQ(0, router.createHandler("/parents", "OPTIONS", 1));
        EXPECT_EQ(0, router.createHandler("parents", "OPTIONS", 1));
        EXPECT_EQ(0, router.createHandler("/////parents", "OPTIONS", 1));
        EXPECT_EQ(0, router.createHandler("/////parents//", "OPTIONS", 1));
        EXPECT_EQ(0, router.createHandler("/////parents//", "OPTIONS", 1));
        // -------------------------------------------------------- //

        // -------------------------------------------------------- //
        EXPECT_EQ(11, router.createHandler("/parents/1234", "OPTIONS", 1));
        EXPECT_EQ(22, router.createHandler("/parents/2345", "GET", 2));
        EXPECT_EQ(33, router.createHandler("/parents/3456", "DELETE", 3));
        EXPECT_EQ(44, router.createHandler("/parents/4567", "PUT", 4));
        EXPECT_EQ(55, router.createHandler("/parents/5678", "PATCH", 5));

        EXPECT_EQ(11, router.createHandler("parents//1234", "OPTIONS", 1));
        EXPECT_EQ(11, router.createHandler("/////parents//1234", "OPTIONS", 1));
        EXPECT_EQ(11, router.createHandler("/////parents//1234//", "OPTIONS", 1));
        // -------------------------------------------------------- //


        // -------------------------------------------------------- //
        EXPECT_EQ(222, router.createHandler("/parents/615243/children?key1=value1", "GET", 22));
        EXPECT_EQ(333, router.createHandler("/parents/273645/children?key2=value2&key1=value1", "DELETE", 33));
        EXPECT_EQ(444, router.createHandler("/parents/837465/children?key1=value1&key2=value1&q=search%20term", "POST", 44));
        EXPECT_EQ(111, router.createHandler("/parents/495867/children?key1=value1&key1=value2&key3=value%203", "OPTIONS", 11));

        EXPECT_EQ(111, router.createHandler("parents/495867/children?key1=value1&key1=value2&key3=value%203", "OPTIONS", 11));
        EXPECT_EQ(111, router.createHandler("parents//495867//children//?&key1=value1&key1=value2&key3=value%203", "OPTIONS", 11));
        EXPECT_EQ(111, router.createHandler("//parents//495867//children//?&key1=value1&&&key1=value2&key3=value%203&&&", "OPTIONS", 11));
        EXPECT_THROW(router.createHandler("//parents//495867//children//?&key1=value1&&&key1=value2&key3=value%203=2&&&", "OPTIONS", 11), std::runtime_error);
        // -------------------------------------------------------- //


        // -------------------------------------------------------- //
        EXPECT_EQ(1111, router.createHandler("/parents/Anna/George/children/Jonny/14/165?key1=value1&key2=value2&key3=value3", "GET", 111));

        EXPECT_EQ(1111, router.createHandler("parents/Anna/George/children/Jonny/14/165?key1=value1&key2=value2&key3=value3&", "GET", 111));
        EXPECT_EQ(1111, router.createHandler("//parents//Anna//George////children///Jonny/14/165////?&&&key1=value1&&&&key2=value2&key3=value3&&&", "GET", 111));
        // -------------------------------------------------------- //
    }
}
