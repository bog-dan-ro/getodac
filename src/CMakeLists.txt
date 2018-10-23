if (SANITIZE_ADDRESS)
    add_definitions(-DBOOST_USE_ASAN)
endif()

add_subdirectory(server)
add_subdirectory(plugins)
