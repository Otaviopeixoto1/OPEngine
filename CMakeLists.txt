cmake_minimum_required(VERSION 3.0.0)
project(OP_Renderer VERSION 0.1.0)

include(ExternalProject)

#configs for building the right version of openGL
find_package(glfw3 REQUIRED)
find_package(OpenGL REQUIRED)
cmake_policy(SET CMP0072 NEW)


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



#add the external libraries "lib/assimp/" "lib/zlib/"
foreach (sub "lib/glad/" "lib/utils/" "lib/glm/" "lib/jsoncpp/" )
    add_subdirectory("${sub}")
endforeach ()

#building assimp from scratch:
include(FetchContent)
set(FETCHCONTENT_BASE_DIR ${PROJECT_SOURCE_DIR}/lib CACHE PATH "Missing description." FORCE)
FetchContent_Declare(assimp
GIT_REPOSITORY https://github.com/assimp/assimp.git
GIT_TAG v5.3.1)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ASSIMP_INJECT_DEBUG_POSTFIX OFF CACHE BOOL "" FORCE)
set(ASSIMP_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(assimp)
target_include_directories(OP_Renderer PRIVATE lib/assimp-src/include)
target_link_libraries(OP_Renderer PRIVATE assimp)



#linking the actual source code of the libraries
target_link_libraries(OP_Renderer PRIVATE
    glfw
    OpenGL::GL
    glad
    utils
    glm
    jsoncpp
    assimp
)

