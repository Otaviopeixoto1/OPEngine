# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.22

# Delete rule output on recipe failure.
.DELETE_ON_ERROR:

#=============================================================================
# Special targets provided by cmake.

# Disable implicit rules so canonical targets will work.
.SUFFIXES:

# Disable VCS-based implicit rules.
% : %,v

# Disable VCS-based implicit rules.
% : RCS/%

# Disable VCS-based implicit rules.
% : RCS/%,v

# Disable VCS-based implicit rules.
% : SCCS/s.%

# Disable VCS-based implicit rules.
% : s.%

.SUFFIXES: .hpux_make_needs_suffix_list

# Command-line flag to silence nested $(MAKE).
$(VERBOSE)MAKESILENT = -s

#Suppress display of executed commands.
$(VERBOSE).SILENT:

# A target that is always out of date.
cmake_force:
.PHONY : cmake_force

#=============================================================================
# Set environment variables for the build.

# The shell in which to execute make rules.
SHELL = /bin/sh

# The CMake executable.
CMAKE_COMMAND = /usr/bin/cmake

# The command to remove a file.
RM = /usr/bin/cmake -E rm -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /home/otavio/openGL/OPEngine

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /home/otavio/openGL/OPEngine/build

# Include any dependencies generated for this target.
include lib/jsoncpp/CMakeFiles/jsoncpp.dir/depend.make
# Include any dependencies generated by the compiler for this target.
include lib/jsoncpp/CMakeFiles/jsoncpp.dir/compiler_depend.make

# Include the progress variables for this target.
include lib/jsoncpp/CMakeFiles/jsoncpp.dir/progress.make

# Include the compile flags for this target's objects.
include lib/jsoncpp/CMakeFiles/jsoncpp.dir/flags.make

lib/jsoncpp/CMakeFiles/jsoncpp.dir/src/jsoncpp.cpp.o: lib/jsoncpp/CMakeFiles/jsoncpp.dir/flags.make
lib/jsoncpp/CMakeFiles/jsoncpp.dir/src/jsoncpp.cpp.o: ../lib/jsoncpp/src/jsoncpp.cpp
lib/jsoncpp/CMakeFiles/jsoncpp.dir/src/jsoncpp.cpp.o: lib/jsoncpp/CMakeFiles/jsoncpp.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/otavio/openGL/OPEngine/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building CXX object lib/jsoncpp/CMakeFiles/jsoncpp.dir/src/jsoncpp.cpp.o"
	cd /home/otavio/openGL/OPEngine/build/lib/jsoncpp && /usr/bin/g++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -MD -MT lib/jsoncpp/CMakeFiles/jsoncpp.dir/src/jsoncpp.cpp.o -MF CMakeFiles/jsoncpp.dir/src/jsoncpp.cpp.o.d -o CMakeFiles/jsoncpp.dir/src/jsoncpp.cpp.o -c /home/otavio/openGL/OPEngine/lib/jsoncpp/src/jsoncpp.cpp

lib/jsoncpp/CMakeFiles/jsoncpp.dir/src/jsoncpp.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/jsoncpp.dir/src/jsoncpp.cpp.i"
	cd /home/otavio/openGL/OPEngine/build/lib/jsoncpp && /usr/bin/g++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/otavio/openGL/OPEngine/lib/jsoncpp/src/jsoncpp.cpp > CMakeFiles/jsoncpp.dir/src/jsoncpp.cpp.i

lib/jsoncpp/CMakeFiles/jsoncpp.dir/src/jsoncpp.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/jsoncpp.dir/src/jsoncpp.cpp.s"
	cd /home/otavio/openGL/OPEngine/build/lib/jsoncpp && /usr/bin/g++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/otavio/openGL/OPEngine/lib/jsoncpp/src/jsoncpp.cpp -o CMakeFiles/jsoncpp.dir/src/jsoncpp.cpp.s

# Object files for target jsoncpp
jsoncpp_OBJECTS = \
"CMakeFiles/jsoncpp.dir/src/jsoncpp.cpp.o"

# External object files for target jsoncpp
jsoncpp_EXTERNAL_OBJECTS =

lib/jsoncpp/libjsoncpp.a: lib/jsoncpp/CMakeFiles/jsoncpp.dir/src/jsoncpp.cpp.o
lib/jsoncpp/libjsoncpp.a: lib/jsoncpp/CMakeFiles/jsoncpp.dir/build.make
lib/jsoncpp/libjsoncpp.a: lib/jsoncpp/CMakeFiles/jsoncpp.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --bold --progress-dir=/home/otavio/openGL/OPEngine/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Linking CXX static library libjsoncpp.a"
	cd /home/otavio/openGL/OPEngine/build/lib/jsoncpp && $(CMAKE_COMMAND) -P CMakeFiles/jsoncpp.dir/cmake_clean_target.cmake
	cd /home/otavio/openGL/OPEngine/build/lib/jsoncpp && $(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/jsoncpp.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
lib/jsoncpp/CMakeFiles/jsoncpp.dir/build: lib/jsoncpp/libjsoncpp.a
.PHONY : lib/jsoncpp/CMakeFiles/jsoncpp.dir/build

lib/jsoncpp/CMakeFiles/jsoncpp.dir/clean:
	cd /home/otavio/openGL/OPEngine/build/lib/jsoncpp && $(CMAKE_COMMAND) -P CMakeFiles/jsoncpp.dir/cmake_clean.cmake
.PHONY : lib/jsoncpp/CMakeFiles/jsoncpp.dir/clean

lib/jsoncpp/CMakeFiles/jsoncpp.dir/depend:
	cd /home/otavio/openGL/OPEngine/build && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/otavio/openGL/OPEngine /home/otavio/openGL/OPEngine/lib/jsoncpp /home/otavio/openGL/OPEngine/build /home/otavio/openGL/OPEngine/build/lib/jsoncpp /home/otavio/openGL/OPEngine/build/lib/jsoncpp/CMakeFiles/jsoncpp.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : lib/jsoncpp/CMakeFiles/jsoncpp.dir/depend
