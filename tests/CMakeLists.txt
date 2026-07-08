include(FetchContent)

# https://github.com/google/googletest/releases/
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.17.0
)

# Windows: prevent overriding parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# googletest is third-party; do not subject it to the project's strict
# -Wall -Wextra -Wpedantic -Werror (or /W4 /WX) flags inherited via
# add_compile_options in cmake/Platform.cmake, or its own sources fail to
# compile (e.g. -Wcharacter-conversion on newer Clang).
foreach(gtest_target gtest gtest_main gmock gmock_main)
    if(TARGET ${gtest_target})
        if(MSVC)
            target_compile_options(${gtest_target} PRIVATE /w)
        else()
            target_compile_options(${gtest_target} PRIVATE -w)
        endif()
    endif()
endforeach()

add_executable(kit_tests main.cpp)

target_include_directories(kit_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(kit_tests PRIVATE kit::kit gtest_main)

add_test(NAME kit_tests COMMAND kit_tests)
