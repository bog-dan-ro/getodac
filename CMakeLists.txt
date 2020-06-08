cmake_minimum_required(VERSION 3.0)

project(getodac)

set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/externals/sanitizers-cmake/cmake" ${CMAKE_MODULE_PATH})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin )
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib/getodac/plugins)

set(CMAKE_MODULE_PATH ${CMAKE_BINARY_DIR} ${CMAKE_MODULE_PATH})

option(ENABLE_TRACE_LOG "Enabe tracing log" OFF)
option(ENABLE_DEBUG_LOG "Enable debugging log" OFF)
option(ENABLE_STRESS_TEST "Enable server stress" OFF)

set(Boost_USE_MULTITHREADED ON)

find_package(Sanitizers)

if (ENABLE_TRACE_LOG)
    add_definitions(-DENABLE_TRACE_LOG)
endif ()
if (ENABLE_DEBUG_LOG)
    add_definitions(-DENABLE_DEBUG_LOG)
endif ()

if (ENABLE_STRESS_TEST)
    add_definitions(-DENABLE_STRESS_TEST)
endif ()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(conf)
add_subdirectory(src)

find_package(GTest)
if (GTEST_FOUND)
    enable_testing()
    add_subdirectory(tests)
endif()
