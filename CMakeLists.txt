cmake_minimum_required(VERSION 3.26) # M56: copy_directory_if_different

project(
    AtomEngine
    VERSION 0.0.18
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Tools and tests go to bin/<config>/; each app to bin/<config>/<app>/, its
# own folder (M108, cmake/AtomApps.cmake).
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# M54 (audit BUILD-001): the game executable and its shaders. Off, the
# engine and game libraries and the unit tests still build - with no
# shader compiler (dxc) needed: what CI runs.
option(ATOM_BUILD_GAME "Build the AtomGame executable and compile its shaders (needs dxc)" ON)

# M67: the game as players get it - a windowed program, no console. Off
# for development (Debug and Release keep their console); the package
# command (Tools/Dist) builds its own folder with it on.
option(ATOM_DISTRIBUTION "Build the game as distributed: a windowed program (no console)" OFF)
# M82: the packaged game has no developer tools - F1/F10, the ATOM_*
# switches (Engine/Core/DevSwitch.h), the test harness, benches and hot
# reload are compiled out. It keeps the log and --diagnostics.
if(ATOM_DISTRIBUTION)
    add_compile_definitions(ATOM_DEV_TOOLS=0)
else()
    add_compile_definitions(ATOM_DEV_TOOLS=1)
endif()

option(
    ATOM_BUILD_PRESENTATION_PROBE
    "Build the private D3D12 presentation investigation probe"
    OFF
)

# M66: the C++ runtime is linked statically into every target (SDL3.dll
# included), so a PC without the Visual C++ Redistributable can start the
# game. Set before the first target, so nothing mixes runtimes.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# M101: Release builds keep their symbols (.pdb) beside the build, so a
# player's crash dump can be read - never installed or packaged. /OPT keeps
# the linker's Release optimisations that /DEBUG would turn off. The
# executables record only their .pdb's name, not the developer's folder
# (/PDBALTPATH, set per app at the end of this file).
if(MSVC)
    add_compile_options($<$<CONFIG:Release>:/Zi>)
    # In the flags string, not add_link_options: MSBuild would escape the %
    # signs and record "%%%Demo.pdb%%%" (seen in M101's crash test).
    string(APPEND CMAKE_EXE_LINKER_FLAGS_RELEASE " /DEBUG /OPT:REF /OPT:ICF")
    string(APPEND CMAKE_SHARED_LINKER_FLAGS_RELEASE " /DEBUG /OPT:REF /OPT:ICF")
endif()

# third party libraries
add_subdirectory(external/SDL)
add_subdirectory(external/glm)
add_subdirectory(external/json)

# Dear ImGui (M41): developer panels, with its SDL3 platform backend and its
# SDL_GPU renderer backend. A plain source library: ImGui ships no CMake.
set(ATOM_IMGUI_DIR "${CMAKE_SOURCE_DIR}/external/imgui")
add_library(
    imgui
    STATIC
    ${ATOM_IMGUI_DIR}/imgui.cpp
    ${ATOM_IMGUI_DIR}/imgui_demo.cpp
    ${ATOM_IMGUI_DIR}/imgui_draw.cpp
    ${ATOM_IMGUI_DIR}/imgui_tables.cpp
    ${ATOM_IMGUI_DIR}/imgui_widgets.cpp
    ${ATOM_IMGUI_DIR}/backends/imgui_impl_sdl3.cpp
    ${ATOM_IMGUI_DIR}/backends/imgui_impl_sdlgpu3.cpp
)
target_include_directories(imgui PUBLIC ${ATOM_IMGUI_DIR} ${ATOM_IMGUI_DIR}/backends)
target_link_libraries(imgui PUBLIC SDL3::SDL3)

# v0.0.15 (M95-M97): CPU profiling zones (Engine/Core/Profile.h) compile to
# nothing by default. -DATOM_TRACE=ON records them with the engine's own
# minimal tracer (Engine/Core/Trace.h): ATOM_TRACE=<prefix> writes a
# Perfetto trace, a zone CSV and the slowest frames. Never in a package.
# ADR-010. AtomProfile carries the switch to every target with zones.
option(ATOM_TRACE "Record the CPU profiling zones with the built-in tracer (development builds only)" OFF)
if(ATOM_TRACE AND ATOM_DISTRIBUTION)
    message(FATAL_ERROR "ATOM_TRACE is for development builds; a distribution build carries no developer tools (ADR-008)")
endif()
add_library(AtomProfile INTERFACE)
target_include_directories(AtomProfile INTERFACE ${CMAKE_SOURCE_DIR}/Engine)
target_compile_definitions(AtomProfile INTERFACE ATOM_PROFILE_TRACE=$<BOOL:${ATOM_TRACE}>)

# The CPU tests (M105): each owner - engine, framework, content, each app -
# declares its own test target (cmake/AtomTesting.cmake), so testing is on
# before any of them is added.
option(ATOM_BUILD_TESTS "Build the CPU tests (AtomTests builds them all)" ON)
if(ATOM_BUILD_TESTS)
    enable_testing()
    include(cmake/AtomTesting.cmake)
endif()

include(cmake/AtomApps.cmake) # M108: atom_app_payload - each app's own runtime folder

if(ATOM_BUILD_GAME)
    add_subdirectory(Shaders)
endif()
add_subdirectory(Engine)
add_subdirectory(Framework) # M76: shared by every game
# The apps (v0.0.16, M109: under Apps/, by what each is for).
add_subdirectory(Apps/Showcase) # v0.0.14: the engine's front door, every feature in one scene
add_subdirectory(Apps/Games/Demo) # v0.0.14: the demo, beside DRIFT
add_subdirectory(Apps/Games/Drift) # M77: a second game
add_subdirectory(Apps/Samples/HelloAtom) # M91: the smallest app - docs/Getting-Started.md
add_subdirectory(Apps/Benchmarks/AtomArena) # M93: a seeded benchmark arena

# M101: each app's executable names its .pdb without a path. Spelled out:
# MSBuild escapes the linker's own %_PDB% into a wrong name.
if(MSVC)
    foreach(app Showcase Demo Drift HelloAtom AtomArena)
        if(TARGET ${app}) # ATOM_BUILD_GAME=OFF (CI) defines only some of them
            target_link_options(${app} PRIVATE $<$<CONFIG:Release>:/PDBALTPATH:$<TARGET_PDB_FILE_NAME:${app}>>)
        endif()
    endforeach()
endif()

add_subdirectory(Tools/LevelCheck) # M112: AtomLevelCheck, levels checked without the game

if(ATOM_BUILD_TESTS)
    add_subdirectory(Tests) # last: the aggregate AtomTests names every owner's
endif()

if(WIN32 AND ATOM_BUILD_PRESENTATION_PROBE)
    add_subdirectory(Tools/PresentationProbe)
endif()
