add_subdirectory(common)

add_subdirectory(library)

find_package(CURL)
if (CURL_FOUND)
    add_subdirectory(server_tests)
endif()
