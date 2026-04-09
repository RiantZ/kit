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

add_executable(kit_tests main.cpp)

target_include_directories(kit_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(kit_tests PRIVATE kit::kit gtest_main)

add_test(NAME kit_tests COMMAND kit_tests)
