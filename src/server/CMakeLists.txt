find_package(Boost 1.57 REQUIRED COMPONENTS coroutine program_options filesystem system)
find_package(OpenSSL REQUIRED)

include_directories(${PROJECT_SOURCE_DIR}/include http-parser ${Boost_INCLUDE_DIRS} ${OPENSSL_INCLUDE_DIR})

set(SRCS http-parser/http_parser.c main.cpp secured_server_session.cpp server.cpp server_plugin.cpp server_service_sessions.cpp server_session.cpp sessions_event_loop.cpp)

add_executable(GETodac ${SRCS})
target_link_libraries(GETodac ${Boost_LIBRARIES} ${OPENSSL_LIBRARIES} ${CMAKE_DL_LIBS})
add_sanitizers(GETodac)

file(GLOB includeFiles "${PROJECT_SOURCE_DIR}/include/getodac/*.h")

set_target_properties(GETodac PROPERTIES PUBLIC_HEADER "${includeFiles}")
install(TARGETS GETodac
        RUNTIME DESTINATION bin
        PUBLIC_HEADER DESTINATION include/getodac)
