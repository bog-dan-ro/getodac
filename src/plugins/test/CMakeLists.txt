find_package(Threads)
find_package(Boost 1.57 REQUIRED COMPONENTS log log_setup)

include_directories(${PROJECT_SOURCE_DIR}/include ${Boost_INCLUDE_DIRS})

add_definitions(-DBOOST_LOG_DYN_LINK -DBOOST_ALL_DYN_LINK)

set(SRCS test_plugin.cpp)

string(APPEND CMAKE_SHARED_LINKER_FLAGS " -Wl,--no-undefined")

add_library(ServerTestsPlugin SHARED ${SRCS})
target_compile_options(ServerTestsPlugin PUBLIC "-fnon-call-exceptions")
target_link_libraries(ServerTestsPlugin ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
add_sanitizers(ServerTestsPlugin)
