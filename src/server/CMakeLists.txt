find_package(Boost 1.57 REQUIRED COMPONENTS coroutine log log_setup program_options system)
find_package(OpenSSL 1.1 REQUIRED)

set(SRCS http-parser/http_parser.c main.cpp
    server.cpp server.h
    serverplugin.cpp serverplugin.h
    serverservicesessions.cpp serverservicesessions.h
    sessionseventloop.cpp sessionseventloop.h
    streams.cpp streams.h
    serversession.cpp serversession.h)

add_executable(GETodac ${SRCS})
target_compile_definitions(GETodac PRIVATE -DHTTP_MAX_HEADER_SIZE=8192 -DBOOST_LOG_DYN_LINK -DBOOST_ALL_DYN_LINK)
target_include_directories(GETodac PRIVATE http-parser ${Boost_INCLUDE_DIRS} ${OPENSSL_INCLUDE_DIR})
target_compile_options(GETodac PRIVATE "-fnon-call-exceptions")
target_link_libraries(GETodac GETodac::dracon ${Boost_LIBRARIES} ${OPENSSL_LIBRARIES} ${CMAKE_DL_LIBS})
target_set_sanitizers(GETodac)

install(TARGETS GETodac
        RUNTIME DESTINATION bin)
