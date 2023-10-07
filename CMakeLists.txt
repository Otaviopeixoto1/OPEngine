cmake_minimum_required(VERSION 3.0.0)
project(my_openGL_proj VERSION 0.1.0)

#configs for building the right version of openGL
find_package(OpenGL REQUIRED)
cmake_policy(SET CMP0072 NEW)

add_executable(my_openGL_proj 
    src/main.cpp 
    src/glad.c include/glad/glad.h
)

#for simpler precompiled binaries
target_link_libraries(my_openGL_proj 
    glfw
    OpenGL::GL
)
#for the custom and 3rd party libraries
target_include_directories(my_openGL_proj PUBLIC include)