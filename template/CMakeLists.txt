cmake_minimum_required(VERSION 3.14)

project(MyCoolProject LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin )
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib/getodac/plugins)
set(CMAKE_PREFIX_PATH ${CMAKE_BINARY_DIR} ${CMAKE_BINARY_DIR}/lib/cmake ${CMAKE_PREFIX_PATH})
set(CMAKE_MODULE_PATH ${CMAKE_BINARY_DIR} ${CMAKE_MODULE_PATH})

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    option(ENABLE_SANITIZERS "Enable sanitizers" ON)
else()
    option(ENABLE_SANITIZERS "Enable sanitizers" OFF)
endif()

option(ENABLE_TRACE_LOG "Enabe tracing log" OFF)
option(ENABLE_DEBUG_LOG "Enable debugging log" OFF)
if (ENABLE_TRACE_LOG)
    add_definitions(-DENABLE_TRACE_LOG)
endif ()
if (ENABLE_DEBUG_LOG)
    add_definitions(-DENABLE_DEBUG_LOG)
endif ()

set(Boost_USE_MULTITHREADED ON)

include(FetchContent)
find_package(GETodac QUIET)
if (NOT TARGET GETodac::dracon)
    message(STATUS "GETodac was not found. Fetching from git")
    FetchContent_Declare(
        GETodac
        GIT_REPOSITORY https://github.com/bog-dan-ro/getodac.git
        GIT_TAG master
        CMAKE_ARGS -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} -DCMAKE_INSTALL_PREFIX:PATH=${CMAKE_BINARY_DIR} -DENABLE_TRACE_LOG=${ENABLE_TRACE_LOG} -DENABLE_DEBUG_LOG=${ENABLE_DEBUG_LOG} -DENABLE_SANITIZERS=${ENABLE_SANITIZERS}
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE
        )
    FetchContent_MakeAvailable(GETodac)
endif()

find_package(nlohmann_json QUIET)
if (NOT TARGET nlohmann_json::nlohmann_json)
    message(STATUS "nlohmann json was not found. Fetching from git")
    FetchContent_Declare(
        nlohmann_json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG v3.10.5
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE
        )
    FetchContent_MakeAvailable(nlohmann_json)
endif()

find_package(Boost 1.66 REQUIRED COMPONENTS log)
include_directories(${Boost_INCLUDE_DIRS})

add_subdirectory(conf)

if (ENABLE_SANITIZERS)
    set (CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=undefined -fsanitize=leak")
    set (CMAKE_LINKER_FLAGS_DEBUG "${CMAKE_LINKER_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=undefined -fsanitize=leak")
    add_definitions(-DBOOST_USE_ASAN)
endif()

add_subdirectory(src)

find_package(CURL)
find_package(GTest)
if (GTEST_FOUND AND CURL_FOUND)
    enable_testing()
    add_subdirectory(tests)
endif()
