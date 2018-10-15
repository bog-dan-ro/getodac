find_package(Boost 1.57 REQUIRED COMPONENTS filesystem iostreams)

include_directories(${PROJECT_SOURCE_DIR}/include http-parser ${Boost_INCLUDE_DIRS})

set(SRCS staticContentPlugin.cpp)

add_library(staticContent SHARED ${SRCS})
target_link_libraries(staticContent ${Boost_LIBRARIES})
add_sanitizers(staticContent)

install(TARGETS staticContent LIBRARY DESTINATION lib/getodac/plugins)
