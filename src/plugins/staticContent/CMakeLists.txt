find_package(Boost 1.57 REQUIRED COMPONENTS iostreams log log_setup)

include_directories(${PROJECT_SOURCE_DIR}/include http-parser ${Boost_INCLUDE_DIRS})

add_definitions(-DBOOST_LOG_DYN_LINK -DBOOST_ALL_DYN_LINK)

set(SRCS static_content_plugin.cpp)

add_library(staticContent SHARED ${SRCS})
target_compile_options(staticContent PUBLIC "-fnon-call-exceptions")
target_link_libraries(staticContent ${Boost_LIBRARIES})
add_sanitizers(staticContent)

install(TARGETS staticContent LIBRARY DESTINATION lib/getodac/plugins)
