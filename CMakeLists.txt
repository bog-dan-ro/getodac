cmake_minimum_required(VERSION 3.0)

project(getodac)

set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/externals/sanitizers-cmake/cmake" ${CMAKE_MODULE_PATH})
set( CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin )
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib/getodac/plugins)

find_package(Sanitizers)

add_subdirectory(conf)
add_subdirectory(src)

find_package(GTest)
if (GTEST_FOUND)
    enable_testing()
    add_subdirectory(tests)
endif()
