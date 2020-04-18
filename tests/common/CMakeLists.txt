project(GETodacTestsLib)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

find_package(CURL)

if (CURL_FOUND)
    include_directories(${CURL_INCLUDE_DIRS})
    set(SRCS EasyCurl.cpp GETodacServer.cpp)
    add_library(GETodacTestsLib STATIC ${SRCS})
    target_link_libraries(GETodacTestsLib ${CURL_LIBRARIES})
    target_include_directories(GETodacTestsLib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CURL_INCLUDE_DIRS})
    file(GLOB includeFiles "${CMAKE_CURRENT_SOURCE_DIR}/*.h")
    set_target_properties(GETodacTestsLib PROPERTIES PUBLIC_HEADER "${includeFiles}")
    install(TARGETS GETodacTestsLib
            ARCHIVE DESTINATION lib/getodac
            PUBLIC_HEADER DESTINATION include/getodac/test)
endif()
