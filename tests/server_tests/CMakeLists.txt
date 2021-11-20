find_package(Boost 1.57 REQUIRED COMPONENTS log system)

include_directories(${GTEST_INCLUDE_DIRS} ${CURL_INCLUDE_DIRS} ${PROJECT_SOURCE_DIR}/include)

set(TEST_SRCS server_tests.cpp ResponseStatusErrorExceptions.cpp Responses.cpp StressServer.cpp Utils.cpp)

add_executable(GETodacServerTests ${TEST_SRCS})
target_link_libraries(GETodacServerTests GETodac::testsLib ${Boost_LIBRARIES} ${GTEST_BOTH_LIBRARIES} pthread)
add_dependencies(GETodacServerTests GETodac::serverTestsPlugin)

add_test(NAME GETodacServerTests COMMAND GETodacServerTests)
