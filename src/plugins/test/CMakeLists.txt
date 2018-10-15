include_directories(${PROJECT_SOURCE_DIR}/include)

set(SRCS test_plugin.cpp)

add_library(tests SHARED ${SRCS})
add_sanitizers(tests)

install(TARGETS tests LIBRARY DESTINATION lib/getodac/plugins)
