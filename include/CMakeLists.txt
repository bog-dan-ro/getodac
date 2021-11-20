add_library(GETodacDracon INTERFACE)
add_library(GETodac::dracon ALIAS GETodacDracon)

target_include_directories(
  GETodacDracon
  INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
            $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

target_compile_features(GETodacDracon INTERFACE cxx_std_17)

install(DIRECTORY dracon TYPE INCLUDE)
