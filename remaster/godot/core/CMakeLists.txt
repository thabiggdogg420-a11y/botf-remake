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
if(EMSCRIPTEN)
    # godot-cpp builds with -sSUPPORT_LONGJMP=wasm, which clang refuses alongside Emscripten's JS exceptions
    # (-fexceptions alone). Native wasm exceptions are compatible with it, for godot-cpp and the core alike.
    string(APPEND CMAKE_CXX_FLAGS " -fwasm-exceptions")
    string(APPEND CMAKE_SHARED_LINKER_FLAGS " -fwasm-exceptions")
endif()
# godot-cpp source: core/godot-cpp (fetch-godot-cpp.sh) by default, or any checkout named by -DBOTF_GODOT_CPP_DIR=
# (build-web.ps1 keeps one shared copy outside the worktrees, so every lane builds the wasm core on its own).
set(BOTF_GODOT_CPP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/godot-cpp" CACHE PATH "godot-cpp source tree")
add_subdirectory(${BOTF_GODOT_CPP_DIR} godot-cpp EXCLUDE_FROM_ALL)

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
if(NOT WIN32 AND NOT EMSCRIPTEN)
    # godot-cpp links libstdc++ statically. Keep its symbols private, or they interpose with the
    # system libstdc++ that the GPU drivers load, and iostream locale facets crash in serialize().
    target_link_options(botf_core PRIVATE -Wl,--exclude-libs,ALL)
endif()
if(WIN32 AND NOT MSVC)
    # One self-contained DLL: no libstdc++/libgcc/winpthread DLLs next to the exe.
    target_link_options(botf_core PRIVATE -static -static-libgcc -static-libstdc++)
endif()
if(MSVC)
    # Native Windows build (VS Build Tools): C++ exceptions, UTF-8 sources, big translation units. /fp:precise is
    # MSVC's default and does not contract, so results match the wasm core. godot-cpp's cached
    # CMAKE_MSVC_RUNTIME_LIBRARY (static CRT) applies here too, so the DLL is self-contained like the mingw one.
    target_compile_options(botf_core PRIVATE /EHsc /utf-8 /bigobj /fp:precise)
endif()

set(BOTF_PLATFORM linux)
set(BOTF_ARCH x86_64)
if(WIN32)
    set(BOTF_PLATFORM windows)
endif()
if(EMSCRIPTEN)
    # Web (docs/DATA-FORMAT.md section 11): a wasm32 side module for Godot's dlink web template, no threads.
    # Configure with: emcmake cmake -S . -B build-web-<target> -G Ninja -DCMAKE_BUILD_TYPE=Release
    #                 -DBOTF_GODOT_TARGET=template_<target> -DGODOTCPP_THREADS=OFF
    # godot-cpp's cmake/web.cmake already adds -sSIDE_MODULE and -sSUPPORT_LONGJMP=wasm.
    set(BOTF_PLATFORM web)
    set(BOTF_ARCH wasm32)
    target_compile_options(botf_core PRIVATE -sSIDE_MODULE=1)
    # Godot's official web templates build the main module with exceptions off (its __cxa_throw aborts). The core
    # throws, so the side module carries its own wasm-exceptions libc++abi and libunwind, bound locally (-Bsymbolic).
    # Build them once with: embuilder build libc++abi-except libunwind-except --pic
    set(BOTF_EM_PIC ${EMSCRIPTEN_SYSROOT}/lib/wasm32-emscripten/pic)
    target_link_options(botf_core PRIVATE -sSIDE_MODULE=1 -sWASM_BIGINT -Wl,-Bsymbolic
        ${BOTF_EM_PIC}/libc++abi-except.a ${BOTF_EM_PIC}/libunwind-except.a)
    # wasm-ld's string tail-merging (-O1 and up) mis-resolves string literal pointers in this side module once the core grows
    # (schema 20: godot-cpp's get_proc_address names read as the tail of another literal and the extension fails to load).
    # -O0 turns the merge off; the module grows by about 14 KB.
    target_link_options(botf_core PRIVATE -Wl,-O0)
    set_target_properties(botf_core PROPERTIES SUFFIX ".wasm")
    # The one symbol still imported is the __cpp_exception tag, which the main module does not define: make it local.
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    add_custom_command(TARGET botf_core POST_BUILD
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/wasm_localize_tag.py $<TARGET_FILE:botf_core>
        VERBATIM)
endif()
set_target_properties(botf_core PROPERTIES
    PREFIX "lib"
    OUTPUT_NAME "botf_core.${BOTF_PLATFORM}.${BOTF_GODOT_TARGET}.${BOTF_ARCH}"
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin)
