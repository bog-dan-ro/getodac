configure_file(server.conf ${CMAKE_BINARY_DIR}/etc/GETodac/server.conf)
configure_file(server_logging.conf ${CMAKE_BINARY_DIR}/etc/GETodac/server_logging.conf)
configure_file(server_ssl_ctx.conf ${CMAKE_BINARY_DIR}/etc/GETodac/server_ssl_ctx.conf)
configure_file(staticFiles.conf ${CMAKE_BINARY_DIR}/etc/GETodac/staticFiles.conf)
configure_file(server.crt ${CMAKE_BINARY_DIR}/etc/GETodac/server.crt COPYONLY)
configure_file(server.key ${CMAKE_BINARY_DIR}/etc/GETodac/server.key COPYONLY)

install(DIRECTORY ${CMAKE_BINARY_DIR}/etc/GETodac/ DESTINATION etc/GETodac
    FILES_MATCHING PATTERN "*.conf"
    PATTERN "CMake*" EXCLUDE)
