cmake_minimum_required(VERSION 3.0.0)
project(OP_Renderer VERSION 0.1.0)

#configs for building the right version of openGL
find_package(OpenGL REQUIRED)
cmake_policy(SET CMP0072 NEW)




#add_executable(OP_Renderer 
#    src/main.cpp 
#    src/common/shader.h
#    src/test/GLtest.h
#)

add_executable(OP_Renderer)

#getting all files in the sources directory
file(GLOB_RECURSE sources ./src/*.cpp)
file(GLOB_RECURSE headers ./src/*.h)

#adding sources separetely
target_sources(OP_Renderer PRIVATE ${sources} ${headers})



#environment settings passed to the c++ code:
configure_file(env.h.in env.h)
#adding the directory of the env header to the include directories:
target_include_directories(OP_Renderer PUBLIC "${PROJECT_BINARY_DIR}")



#directory containing a library
#add_subdirectory(lib/glad/)
foreach (sub "lib/glad/" "lib/utils/" "lib/glm/")
    add_subdirectory("${sub}")
endforeach ()

#for linking the actual source code of the libraries
target_link_libraries(OP_Renderer PRIVATE
    glfw
    OpenGL::GL
    glad
    utils
    glm
)

