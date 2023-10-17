cmake_minimum_required(VERSION 3.0.0)
project(assimp)

#creating a library based on an already built static library:

#using Assimp 5.3.1
add_library(assimp STATIC IMPORTED GLOBAL)
set_target_properties(assimp PROPERTIES IMPORTED_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/libassimp.a)
target_include_directories(assimp INTERFACE include/)