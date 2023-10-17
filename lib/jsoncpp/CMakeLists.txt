cmake_minimum_required(VERSION 3.0.0)
project(jsoncpp)

#the full library (src + include/headers) can be handeled in this file and it can be included as subdir in the main
#CMakeLists and linked as a library to the main target
add_library(jsoncpp include/json/json.h src/jsoncpp.cpp)
target_include_directories(jsoncpp PUBLIC include/)