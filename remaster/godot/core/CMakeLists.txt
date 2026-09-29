cmake_minimum_required(VERSION 3.20)
project(BotfGodotCore LANGUAGES CXX)

# GDExtension "botf_core": the deterministic simulation core plus the web JSON bridge
# (remaster/web/bridge/WebBridge.cpp), compiled natively and exposed to GDScript as the
# BotfCore engine singleton. Build with ./build.sh (linux, windows, or both).
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# The core throws for invalid orders and the bridge returns those messages as JSON errors.
set(GODOTCPP_DISABLE_EXCEPTIONS OFF CACHE BOOL "" FORCE)
# template_debug for the editor, headless tests and debug exports; template_release for release exports.
set(BOTF_GODOT_TARGET template_debug CACHE STRING "template_debug or template_release")
set(GODOTCPP_TARGET ${BOTF_GODOT_TARGET} CACHE STRING "" FORCE)
add_subdirectory(godot-cpp EXCLUDE_FROM_ALL)

set(REMASTER ${CMAKE_CURRENT_SOURCE_DIR}/../..)
set(BOTF_CORE_DIR ${REMASTER}/game/Source/BotfSim)
# Every core translation unit except the CLI entry point (same set as web/build-wasm.sh).
file(GLOB BOTF_CORE_SOURCES CONFIGURE_DEPENDS ${BOTF_CORE_DIR}/*.cpp)
list(FILTER BOTF_CORE_SOURCES EXCLUDE REGEX "/Main\\.cpp$")

add_library(botf_core SHARED
    src/register_types.cpp
    src/botf_core.cpp
    ${BOTF_CORE_SOURCES}
    ${REMASTER}/web/bridge/WebBridge.cpp)
target_include_directories(botf_core PRIVATE ${BOTF_CORE_DIR} ${REMASTER}/web/bridge)
target_link_libraries(botf_core PRIVATE godot-cpp)
if(NOT MSVC)
    # No -ffast-math and no FP contraction, so results match the native and wasm core bit for bit.
    target_compile_options(botf_core PRIVATE -fexceptions -ffp-contract=off)
endif()
if(WIN32)
    # One self-contained DLL: no libstdc++/libgcc/winpthread DLLs next to the exe.
    target_link_options(botf_core PRIVATE -static -static-libgcc -static-libstdc++)
endif()

set(BOTF_PLATFORM linux)
if(WIN32)
    set(BOTF_PLATFORM windows)
endif()
set_target_properties(botf_core PROPERTIES
    PREFIX "lib"
    OUTPUT_NAME "botf_core.${BOTF_PLATFORM}.${BOTF_GODOT_TARGET}.x86_64"
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin)
