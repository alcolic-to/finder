# Usage:
# Create initial cmake build folder: cmake -S . -B .\build
# Create build with desired config: cmake --build .\build --config [Debug/Release/RelWithDebInfo/MinSizeRel]
#
# Clean command: cmake --build .\build --target clean

# If you want to play around with scheduler, you can just enable BUILD_MAIN which builds main.cpp file.

# To enable tracy profiler, build project with ENABLE_TRACY=ON and make sure to have debug symbols (e.g. RelWithDebugInfo)
# You will also need tracy server, GUI etc. installed, since we are not build it from scratch (I tried couple of times and it failed, so I gave up).
# For windows, found needed binaries under releases https://github.com/wolfpld/tracy. Note that server (downloaded binaries) and client version (one compiled by UMS) needs to be the same.
# Also, on windows, make sure to run program as administrator so tracy can collect stack traces.

cmake_minimum_required(VERSION 3.10)
project(trie)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON) # -> compile_commands.json

# Separate bin directory from build directory.
#
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/$<CONFIG>)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib/$<CONFIG>)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib/$<CONFIG>)

include(cmake/project_config.cmake)
include(cmake/sanitizers.cmake)
include(cmake/compiler_options.cmake)

set(CPP_FILES main.cpp art.cpp)
set(HPP_FILES )
set(ALL_FILES ${CPP_FILES} ${HPP_FILES})

add_executable(trie ${CPP_FILES})

include(CTest)
