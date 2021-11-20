cmake_minimum_required(VERSION 3.12)

project(GETodac LANGUAGES CXX C VERSION 1.0)

set(BUILD_SHARED_LIBS OFF)

include(GNUInstallDirs)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin )
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib/getodac/plugins)
set(CMAKE_MODULE_PATH ${CMAKE_BINARY_DIR} ${CMAKE_MODULE_PATH})

option(ENABLE_TRACE_LOG "Enabe tracing log" OFF)
option(ENABLE_DEBUG_LOG "Enable debugging log" OFF)

set(Boost_USE_MULTITHREADED ON)

if (ENABLE_TRACE_LOG)
    add_definitions(-DENABLE_TRACE_LOG)
endif ()
if (ENABLE_DEBUG_LOG)
    add_definitions(-DENABLE_DEBUG_LOG)
endif ()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    option(ENABLE_SANITIZERS "Enable GETodac sanitizers" ON)
else()
    option(ENABLE_SANITIZERS "Enable GETodac sanitizers" OFF)
endif()

macro(target_set_sanitizers target)
    if (ENABLE_SANITIZERS)
        set(sanitizers_flags -fno-omit-frame-pointer -fsanitize=undefined -fsanitize=leak)
        target_compile_definitions(${target} PUBLIC -DBOOST_USE_ASAN)
        target_compile_options(${target} PUBLIC ${sanitizers_flags})
        target_link_options(${target} PUBLIC ${sanitizers_flags})
    endif()
endmacro()

add_subdirectory(conf)
add_subdirectory(include)
add_subdirectory(src)

option(ENABLE_TESTS "Enable GETodac tests" ON)
if (ENABLE_TESTS)
    find_package(GTest)
    if (GTEST_FOUND)
        enable_testing()
        add_subdirectory(tests)
    endif()
endif()
