include_directories(${GTEST_INCLUDE_DIRS} ${PROJECT_SOURCE_DIR}/include)

add_executable(GETodacTests GETodacTests.cpp GETodacUtils.cpp GETodacRESTfull.cpp)
target_link_libraries(GETodacTests ${GTEST_BOTH_LIBRARIES} pthread)
add_test(AllTestsInFoo GETodacTests)
