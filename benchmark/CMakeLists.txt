OPTION(FINDER_BUILD_BENCHMARK "If set, builds all benchmarks." OFF)
if (NOT FINDER_BUILD_BENCHMARK)
    return ()
endif()

include(FetchContent)

FetchContent_Declare(
    benchmark
    GIT_REPOSITORY https://github.com/google/benchmark.git
    GIT_TAG        v1.9.1)

set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(benchmark)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/$<CONFIG>/benchmark)

message("Disabling clang-tidy for google benchmark.")
set_property(TARGET benchmark PROPERTY CXX_CLANG_TIDY "")
set_property(TARGET benchmark_main PROPERTY CXX_CLANG_TIDY "")

function(add_finder_benchmark BENCHMARK_FILE)
    get_filename_component(BENCHMARK_NAME ${BENCHMARK_FILE} NAME_WE)
    add_executable(${BENCHMARK_NAME} ${BENCHMARK_FILE})
    target_link_libraries(${BENCHMARK_NAME} PRIVATE benchmark::benchmark)
    target_include_directories(${BENCHMARK_NAME} PUBLIC ${FINDER_STL_PATH} ${CMAKE_SOURCE_DIR})
endfunction()

# add_finder_benchmark("my_benchmark.cpp")