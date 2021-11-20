find_package(Boost 1.57 REQUIRED COMPONENTS iostreams log log_setup)

add_definitions(-DBOOST_LOG_DYN_LINK -DBOOST_ALL_DYN_LINK)

set(SRCS static_content_plugin.cpp)

add_library(StaticContent SHARED ${SRCS})
add_library(GETodac::staticContent ALIAS StaticContent)
target_compile_options(StaticContent PUBLIC "-fnon-call-exceptions")
target_link_libraries(StaticContent GETodac::dracon Boost::iostreams Boost::log Boost::log_setup)
target_set_sanitizers(StaticContent)

install(TARGETS StaticContent LIBRARY DESTINATION lib/getodac/plugins)
