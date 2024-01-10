cmake_minimum_required(VERSION 3.0.0)
project(imgui)

#the full library (src + include/headers) can be handeled in this file and it can be included as subdir in the main
#CMakeLists and linked as a library to the main target
add_library(
    #headers:
    imgui include/imconfig.h include/imgui_internal.h include/imgui.h include/imstb_rectpack.h include/imstb_textedit.h include/imstb_truetype.h include/imgui_impl_glfw.h include/imgui_impl_opengl3.h include/imgui_impl_opengl3_loader.h
    #imgui functions:
    src/imgui_demo.cpp src/imgui_draw.cpp src/imgui_tables.cpp src/imgui_widgets.cpp src/imgui.cpp 
    #backends:
    src/imgui_impl_opengl3.cpp src/imgui_impl_glfw.cpp)
target_include_directories(imgui PUBLIC include/)