configure_file(${PROJECT_NAME}.conf ${CMAKE_BINARY_DIR}/etc/GETodac/${PROJECT_NAME}.conf)
configure_file(server.crt ${CMAKE_BINARY_DIR}/etc/GETodac/server.crt COPYONLY)
configure_file(server.key ${CMAKE_BINARY_DIR}/etc/GETodac/server.key COPYONLY)

install(FILES ${PROJECT_NAME}.conf DESTINATION etc/GETodac)
