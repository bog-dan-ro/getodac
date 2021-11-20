set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(SRCS EasyCurl.cpp GETodacServer.cpp)

add_library(GETodacTestsLib STATIC ${SRCS})
add_library(GETodac::testsLib ALIAS GETodacTestsLib)

target_link_libraries(GETodacTestsLib ${CURL_LIBRARIES})
target_include_directories(GETodacTestsLib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CURL_INCLUDE_DIRS})
target_compile_features(GETodacTestsLib PUBLIC cxx_std_17)
target_set_sanitizers(GETodacTestsLib)

file(GLOB includeFiles "${CMAKE_CURRENT_SOURCE_DIR}/*.h")
set_target_properties(GETodacTestsLib PROPERTIES PUBLIC_HEADER "${includeFiles}")
install(TARGETS GETodacTestsLib
        ARCHIVE DESTINATION lib/dracon
        PUBLIC_HEADER DESTINATION include/dracon/test)
