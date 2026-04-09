cmake_minimum_required(VERSION 4.0)
project(kit VERSION 0.1.0 LANGUAGES C CXX)

if(PROJECT_IS_TOP_LEVEL AND NOT KIT_BUILD_SCRIPT)
    message(FATAL_ERROR
        "Invoke CMake via the build script:\n"
        "  python scripts/cmake_build.py [--build] [--clean]"
    )
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(cmake/Platform.cmake)

add_subdirectory(src)

if(PROJECT_IS_TOP_LEVEL)
    option(KIT_BUILD_TESTS "Build tests" ON)
else()
    option(KIT_BUILD_TESTS "Build tests" OFF)
endif()

if(KIT_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
