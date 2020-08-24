find_package(Boost 1.57 REQUIRED COMPONENTS coroutine filesystem log log_setup program_options system)
find_package(OpenSSL 1.1 REQUIRED)

include_directories(${PROJECT_SOURCE_DIR}/include http-parser ${Boost_INCLUDE_DIRS} ${OPENSSL_INCLUDE_DIR})

add_definitions(-DHTTP_MAX_HEADER_SIZE=8192)
add_definitions(-DBOOST_LOG_DYN_LINK -DBOOST_ALL_DYN_LINK)

set(SRCS http-parser/http_parser.c main.cpp server.cpp server_plugin.cpp server_service_sessions.cpp sessions_event_loop.cpp streams.cpp streams.h server_session.cpp)

add_executable(GETodac ${SRCS})
target_compile_options(GETodac PUBLIC "-fnon-call-exceptions")
target_link_libraries(GETodac ${Boost_LIBRARIES} ${OPENSSL_LIBRARIES} ${CMAKE_DL_LIBS})
add_sanitizers(GETodac)

file(GLOB includeFiles "${PROJECT_SOURCE_DIR}/include/dracon/*.h")
set_target_properties(GETodac PROPERTIES PUBLIC_HEADER "${includeFiles}")
install(TARGETS GETodac
        RUNTIME DESTINATION bin
        PUBLIC_HEADER DESTINATION include/dracon)
