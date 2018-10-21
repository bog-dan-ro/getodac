find_package(Boost 1.57 REQUIRED COMPONENTS system filesystem)

include_directories(${GTEST_INCLUDE_DIRS} ${CURL_INCLUDE_DIRS} ${Boost_INCLUDE_DIRS} ${PROJECT_SOURCE_DIR}/include)

set(TEST_SRCS server_tests.cpp global.cpp ResponseStatusErrorExceptions.cpp)

add_executable(GETodacServerTests ${TEST_SRCS})
add_sanitizers(GETodacServerTests)
target_link_libraries(GETodacServerTests ${CURL_LIBRARIES} ${Boost_LIBRARIES} ${GTEST_BOTH_LIBRARIES} pthread)
add_test(AllGETodacTests GETodacServerTests)
add_dependencies(GETodacServerTests ServerTestsPlugin)
