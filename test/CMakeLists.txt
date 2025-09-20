OPTION(FINDER_BUILD_TEST "If set, builds all tests." OFF)
if (NOT FINDER_BUILD_TEST)
    return()
endif()

include(FetchContent)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        release-1.11.0)

# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(googletest)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/$<CONFIG>/test)

message("Disabling clang-tidy for google tests.")
set_target_properties(gmock PROPERTIES CXX_CLANG_TIDY "")
set_target_properties(gmock_main PROPERTIES CXX_CLANG_TIDY "")
set_target_properties(gtest PROPERTIES CXX_CLANG_TIDY "")
set_target_properties(gtest_main PROPERTIES CXX_CLANG_TIDY "")

function(add_gtest TEST_FILE)
    get_filename_component(TEST_NAME ${TEST_FILE} NAME_WE)
    add_executable(${TEST_NAME} ${TEST_FILE})
    target_link_libraries(${TEST_NAME} ${ART_LIBRARIES} gtest_main)
    target_include_directories(${TEST_NAME} PUBLIC ${CMAKE_SOURCE_DIR})
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
    target_compile_definitions(${TEST_NAME} PRIVATE PROJECT_ROOT="${CMAKE_SOURCE_DIR}")
endfunction()

add_gtest("art_test.cpp")
add_gtest("key_value_test.cpp")
# add_gtest("fs_trie_test.cpp")
add_gtest("files_test.cpp")
add_gtest("small_string_test.cpp")
add_gtest("ast_test.cpp")
