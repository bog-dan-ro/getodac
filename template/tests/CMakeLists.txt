include_directories(${GTEST_INCLUDE_DIRS} ${CURL_INCLUDE_DIRS})
include_directories(${CMAKE_SOURCE_DIR}/src)

set (CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address")
set (CMAKE_LINKER_FLAGS_DEBUG "${CMAKE_LINKER_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address")

set(TEST_SRCS main.cpp devices.cpp)

add_executable(${PROJECT_NAME}Tests ${TEST_SRCS})
target_link_libraries(${PROJECT_NAME}Tests GETodac::testsLib ${OPENSSL_LIBRARIES} ${CURL_LIBRARIES} ${Boost_LIBRARIES} ${GTEST_BOTH_LIBRARIES} pthread)
add_dependencies(${PROJECT_NAME}Tests ${PROJECT_NAME})
add_test(NAME ${PROJECT_NAME}Tests COMMAND ${PROJECT_NAME}Tests)
