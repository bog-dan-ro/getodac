cmake_minimum_required(VERSION 3.0)

project(getodac)

set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/externals/sanitizers-cmake/cmake" ${CMAKE_MODULE_PATH})

find_package(Sanitizers)

add_subdirectory(src)
