cmake_minimum_required(VERSION 3.26) # M56: copy_directory_if_different

project(
    AtomEngine
    VERSION 0.0.13
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

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

if(ATOM_BUILD_GAME)
    add_subdirectory(Shaders)
endif()
add_subdirectory(Engine)
add_subdirectory(Framework) # M76: shared by every game
add_subdirectory(Showcase) # v0.0.14: the engine's front door, every feature in one scene
add_subdirectory(Games/Demo) # v0.0.14: the demo, beside DRIFT
add_subdirectory(Games/Drift) # M77: a second game
add_subdirectory(Samples/HelloAtom) # M91: the smallest app - docs/Getting-Started.md

option(ATOM_BUILD_TESTS "Build the unit tests (AtomTests)" ON)
if(ATOM_BUILD_TESTS)
    enable_testing()
    add_subdirectory(Tests)
endif()

if(WIN32 AND ATOM_BUILD_PRESENTATION_PROBE)
    add_subdirectory(Tools/PresentationProbe)
endif()
