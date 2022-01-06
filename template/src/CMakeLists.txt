add_definitions(-DBOOST_LOG_DYN_LINK -DBOOST_ALL_DYN_LINK)

string(APPEND CMAKE_SHARED_LINKER_FLAGS " -Wl,--no-undefined")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib/getodac/plugins)

set(SRCS main.cpp)

add_library(${PROJECT_NAME} SHARED ${SRCS})

target_compile_options(${PROJECT_NAME} PUBLIC "-fnon-call-exceptions")
target_link_directories(${PROJECT_NAME} PRIVATE "${CMAKE_BINARY_DIR}/lib")
target_link_libraries(${PROJECT_NAME} PRIVATE GETodac::dracon nlohmann_json::nlohmann_json ${Boost_LIBRARIES})

install(TARGETS ${PROJECT_NAME} LIBRARY DESTINATION lib/getodac/plugins)
