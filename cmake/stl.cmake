# Sets include directory for external STL lib.
# User can provide custom STL lib path if it is already installed by setting FINDER_STL_PATH variable.
set(FINDER_STL_PATH "" CACHE STRING "Path to STL common lib.")
if (FINDER_STL_PATH STREQUAL "")
    include(FetchContent)

    FetchContent_Declare(
        stl
        GIT_REPOSITORY https://github.com/alcolic-to/stl.git
        GIT_SHALLOW    TRUE
        GIT_PROGRESS   TRUE)

    FetchContent_MakeAvailable(stl)

    message("Using internal STL lib ${stl_SOURCE_DIR} for FINDER.")
    set(FINDER_STL_PATH ${stl_SOURCE_DIR})
    message("FINDER_STL_PATH value: ${FINDER_STL_PATH}")
else ()
    set(FINDER_STL_PATH ${FINDER_STL_PATH})
    message("Using external STL lib ${FINDER_STL_PATH} for FINER.")
endif ()

target_include_directories(finder SYSTEM PUBLIC ${FINDER_STL_PATH})
