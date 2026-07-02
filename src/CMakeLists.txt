set(KIT_SOURCES
    version.cpp
)

if(KIT_PLATFORM_LINUX)
    list(APPEND KIT_SOURCES shared/shared_linux.cpp time/time_linux.cpp system/system_linux.cpp event/event_linux.cpp thread/thread_linux.cpp file/file_linux.cpp)
elseif(KIT_PLATFORM_DARWIN)
    list(APPEND KIT_SOURCES shared/shared_macos.cpp time/time_macos.cpp system/system_macos.cpp event/event_macos.cpp thread/thread_macos.cpp file/file_macos.cpp)
elseif(KIT_PLATFORM_WINDOWS)
    list(APPEND KIT_SOURCES shared/shared_win32.cpp time/time_win32.cpp system/system_win32.cpp event/event_win32.cpp thread/thread_win32.cpp file/file_win32.cpp)
endif()

add_library(kit_obj OBJECT ${KIT_SOURCES})

# The same objects feed both kit_static and kit_shared. Building them with
# position-independent code lets shared-library linkage handle PC-relative
# relocations against exported symbols (e.g. c_file's vtable) on Linux/ELF.
set_target_properties(kit_obj PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_include_directories(kit_obj
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${PROJECT_SOURCE_DIR}/src
)

target_compile_definitions(kit_obj PRIVATE KIT_BUILDING)

configure_file(
    ${PROJECT_SOURCE_DIR}/include/kit/version.h.in
    ${PROJECT_BINARY_DIR}/include/kit/version.h
    @ONLY
)

# Static library
add_library(kit_static STATIC $<TARGET_OBJECTS:kit_obj>)
target_link_libraries(kit_static PUBLIC kit_obj)
target_compile_definitions(kit_static PUBLIC KIT_STATIC)
set_target_properties(kit_static PROPERTIES OUTPUT_NAME kit)

# Shared library
add_library(kit_shared SHARED $<TARGET_OBJECTS:kit_obj>)
target_link_libraries(kit_shared PUBLIC kit_obj)

if(KIT_PLATFORM_LINUX)
    target_link_libraries(kit_shared PRIVATE rt pthread)
    target_link_libraries(kit_static PRIVATE rt pthread)
endif()

set_target_properties(kit_shared PROPERTIES
    OUTPUT_NAME kit
    VERSION     ${PROJECT_VERSION}
    SOVERSION   ${PROJECT_VERSION_MAJOR}
)

# Namespace alias (consumers use kit::kit)
add_library(kit::kit ALIAS kit_static)
