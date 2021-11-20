set(TEST_SRCS GETodacTests.cpp GETodacUtils.cpp GETodacRESTfullRoute.cpp)

add_executable(GETodacLibrartyTests ${TEST_SRCS})
target_link_libraries(GETodacLibrartyTests GETodac::testsLib GETodac::dracon gtest pthread)
target_include_directories(GETodacTestsLib PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
add_test(NAME GETodacLibrartyTests COMMAND GETodacLibrartyTests)
