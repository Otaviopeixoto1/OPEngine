cmake_minimum_required(VERSION 3.0.0)
project(zlib)

#creating a library based on an already built static library:

#using Assimp 5.3.1
add_library(zlib STATIC IMPORTED GLOBAL)
set_target_properties(zlib PROPERTIES IMPORTED_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/libzlibstatic.a)
target_include_directories(zlib INTERFACE include/)