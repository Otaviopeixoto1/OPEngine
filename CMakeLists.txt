cmake_minimum_required(VERSION 3.0.0)
project(OP_renderer VERSION 0.1.0)

#configs for building the right version of openGL
find_package(OpenGL REQUIRED)
cmake_policy(SET CMP0072 NEW)

add_executable(OP_renderer 
    src/main.cpp src/common/shader.h
)


#directory containing a library
add_subdirectory(lib/glad/)


#for linking the actual source code of the libraries
target_link_libraries(OP_renderer PRIVATE
    glfw
    OpenGL::GL
    glad
)

