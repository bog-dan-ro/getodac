find_package(Threads)

include_directories(${PROJECT_SOURCE_DIR}/include)

set(SRCS test_plugin.cpp)

string(APPEND CMAKE_SHARED_LINKER_FLAGS " -Wl,--no-undefined")

add_library(ServerTestsPlugin SHARED ${SRCS})
target_compile_options(ServerTestsPlugin PUBLIC "-fnon-call-exceptions")
target_link_libraries(ServerTestsPlugin ${CMAKE_THREAD_LIBS_INIT})
add_sanitizers(ServerTestsPlugin)
