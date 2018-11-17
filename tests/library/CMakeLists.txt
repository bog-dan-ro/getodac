include_directories(${GTEST_INCLUDE_DIRS} ${PROJECT_SOURCE_DIR}/include)

set(TEST_SRCS GETodacTests.cpp GETodacUtils.cpp GETodacRESTful.cpp GETodacOStreamBuffer.cpp)

add_executable(GETodacLibrartyTests ${TEST_SRCS})
add_sanitizers(GETodacLibrartyTests)
target_link_libraries(GETodacLibrartyTests ${GTEST_BOTH_LIBRARIES} pthread)
add_test(NAME GETodacLibrartyTests COMMAND GETodacLibrartyTests)
