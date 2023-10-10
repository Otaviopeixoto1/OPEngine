cmake_minimum_required(VERSION 3.0.0)
project(utils)


include(GNUInstallDirs)

#INTERFACE is used for header-only libraries 
add_library(utils INTERFACE)

target_include_directories(utils INTERFACE 
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)