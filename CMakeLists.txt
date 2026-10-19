cmake_minimum_required(VERSION 3.26) # M56: copy_directory_if_different

project(
    AtomEngine
    VERSION 0.0.19
    LANGUAGES CXX
)

# v0.0.20 (M127): the engine is also a subproject - a game's own CMake adds
# it with add_subdirectory(<engine> atom) and owns the build. Everything
# here is therefore relative to this folder, never to CMAKE_SOURCE_DIR
# (the game's, then), and what a game's code must agree on travels with
# the targets (usage requirements), not as settings of this folder.
# Cached so functions called from the game's folder see them too.
set(ATOM_ENGINE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}" CACHE INTERNAL "The AtomEngine source folder")
set(ATOM_CONTENT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Content" CACHE INTERNAL "Assets several apps share")

# The engine's own targets; a game gets C++20 from AtomEngine's usage
# requirements (Engine/CMakeLists.txt).
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Tools and tests go to bin/<config>/; each app to bin/<config>/<app>/, its
# own folder (M108, cmake/AtomApps.cmake). Under a game, its build's bin/.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# M54 (audit BUILD-001): the game executable and its shaders. Off, the
# engine and game libraries and the unit tests still build - with no
# shader compiler (dxc) needed: what CI runs.
option(ATOM_BUILD_GAME "Build the AtomGame executable and compile its shaders (needs dxc)" ON)
# M127: the shaders on their own - a game built on the engine needs them
# even with the bundled apps off. Follows ATOM_BUILD_GAME unless set.
option(ATOM_BUILD_SHADERS "Compile the engine's shaders (needs dxc)" ${ATOM_BUILD_GAME})
# M127: what only the engine's own development needs. On when the engine is
# the top-level project; off when a game adds it below its own CMake.
option(ATOM_EXAMPLE_APPS "Build the bundled apps (Showcase, Demo, DRIFT, HelloAtom, AtomArena)" ${PROJECT_IS_TOP_LEVEL})
option(ATOM_BUILD_TOOLS "Build AtomLevelCheck and AtomProjectTool" ${PROJECT_IS_TOP_LEVEL})
option(ATOM_BUILD_WORKSPACE "Build Atom Workspace" ${PROJECT_IS_TOP_LEVEL})
option(ATOM_BUILD_TESTS "Build the CPU tests (AtomTests builds them all)" ${PROJECT_IS_TOP_LEVEL})

# M67: the game as players get it - a windowed program, no console. Off
# for development (Debug and Release keep their console); the package
# command (Tools/Dist) builds its own folder with it on.
option(ATOM_DISTRIBUTION "Build the game as distributed: a windowed program (no console)" OFF)
# M82: the packaged game has no developer tools - F1, the ATOM_* switches
# (Engine/Core/DevSwitch.h), the test harness, benches and hot reload are
# compiled out. It keeps the log and --diagnostics. M127: a PUBLIC
# definition of AtomEngine (Engine/CMakeLists.txt), so every target that
# includes the engine's headers - a game's too - compiles them alike.
if(ATOM_DISTRIBUTION)
    set(ATOM_DEV_TOOLS_VALUE 0)
else()
    set(ATOM_DEV_TOOLS_VALUE 1)
endif()

option(
    ATOM_BUILD_PRESENTATION_PROBE
    "Build the private D3D12 presentation investigation probe"
    OFF
)

# M66: the C++ runtime is linked statically into every target (SDL3.dll
# included), so a PC without the Visual C++ Redistributable can start the
# game. Set before the first target, so nothing mixes runtimes. M127: a
# game's own targets are outside this folder; atom_app_payload sets the same
# runtime on them (cmake/AtomApps.cmake) - a mismatch is LNK2038.
set(ATOM_MSVC_RUNTIME "MultiThreaded$<$<CONFIG:Debug>:Debug>" CACHE INTERNAL "The C++ runtime every AtomEngine target links")
set(CMAKE_MSVC_RUNTIME_LIBRARY "${ATOM_MSVC_RUNTIME}")

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

# Dear ImGui (M41): the workspace's panels, with its SDL3 platform backend
# and its SDL_GPU renderer backend. A plain source library: ImGui ships no
# CMake. Only the workspace (and its tests) use it - a game never builds it.
if(ATOM_BUILD_WORKSPACE OR ATOM_BUILD_TESTS)
    set(ATOM_IMGUI_DIR "${ATOM_ENGINE_ROOT}/external/imgui")
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
endif()

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
target_include_directories(AtomProfile INTERFACE ${ATOM_ENGINE_ROOT}/Engine)
target_compile_definitions(AtomProfile INTERFACE ATOM_PROFILE_TRACE=$<BOOL:${ATOM_TRACE}>)

# The CPU tests (M105): each owner - engine, framework, content, each app -
# declares its own test target (cmake/AtomTesting.cmake), so testing is on
# before any of them is added.
if(ATOM_BUILD_TESTS) # declared with the other options, above
    enable_testing()
    include(cmake/AtomTesting.cmake)
endif()
# The tests check the tools (LevelCheck, project creation) and the workspace's parts.
if(ATOM_BUILD_TESTS)
    set(ATOM_BUILD_TOOLS ON)
endif()

include(cmake/AtomApps.cmake) # M108: atom_app_payload - each app's own runtime folder
include(cmake/AtomProject.cmake) # M128: atom_add_project - a project's build from its descriptor

if(ATOM_BUILD_SHADERS)
    add_subdirectory(Shaders)
endif()
add_subdirectory(Engine)
add_subdirectory(Framework) # M76: shared by every game
add_subdirectory(Authoring) # v0.0.19: what the tools know about content; no game links it
add_subdirectory(Project) # v0.0.19: project descriptors and creation; no GPU, no engine
if(ATOM_BUILD_WORKSPACE OR ATOM_BUILD_TESTS)
    add_subdirectory(WorkspaceUI) # v0.0.19: the workspace's ImGui host - the only ImGui user
endif()

# v0.0.19: the runtime carries no tool code. A guard, not a convention:
# configuring fails if ImGui reaches the engine, the framework or the
# authoring model again (the workspace's UI is the only place for it).
foreach(runtime AtomEngine AtomFramework AtomAuthoring)
    get_target_property(links ${runtime} LINK_LIBRARIES)
    if(links AND "imgui" IN_LIST links)
        message(FATAL_ERROR "${runtime} links imgui: tool UI belongs in WorkspaceUI (v0.0.19)")
    endif()
endforeach()
# The bundled apps (v0.0.16, M109: under Apps/, by what each is for) -
# the examples, listed by hand (v0.0.19: each also has a descriptor, so
# the workspace and AtomLevelCheck find it, but CMake adds them here).
# ATOM_EXAMPLE_APPS is declared with the other options, above.
set(ATOM_EXAMPLE_DIRS
    Apps/Showcase # v0.0.14: the engine's front door, every feature in one scene
    Apps/Games/Demo # v0.0.14: the demo, beside DRIFT
    Apps/Games/Drift # M77: a second game
    Apps/Samples/HelloAtom # M91: the smallest app - docs/Getting-Started.md
    Apps/Benchmarks/AtomArena) # M93: a seeded benchmark arena
if(ATOM_EXAMPLE_APPS)
    foreach(example IN LISTS ATOM_EXAMPLE_DIRS)
        add_subdirectory(${example})
    endforeach()
endif()

# v0.0.19: projects are found, not listed - every <Name>.atomproject.json
# one folder down in ATOM_PROJECTS_DIR (Apps/<Name>/, as AtomProjectTool
# makes them). A project made in the workspace appears at the next
# configure. The examples above have descriptors too, but are skipped.
set(ATOM_PROJECTS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Apps" CACHE PATH "Where AtomEngine projects are discovered")
set(ATOM_PROJECT_DESCRIPTORS "")
if(PROJECT_IS_TOP_LEVEL) # under a game, the game is the project
    file(GLOB ATOM_PROJECT_DESCRIPTORS CONFIGURE_DEPENDS "${ATOM_PROJECTS_DIR}/*/*.atomproject.json")
endif()
foreach(descriptor IN LISTS ATOM_PROJECT_DESCRIPTORS)
    get_filename_component(project_dir "${descriptor}" DIRECTORY)
    file(RELATIVE_PATH project_rel "${CMAKE_CURRENT_SOURCE_DIR}" "${project_dir}")
    if(project_rel IN_LIST ATOM_EXAMPLE_DIRS)
        continue()
    endif()
    get_filename_component(project_name "${project_dir}" NAME)
    message(STATUS "Project: ${project_name} (${project_dir})")
    add_subdirectory("${project_dir}" "${CMAKE_CURRENT_BINARY_DIR}/Projects/${project_name}")
endforeach()

# M101: each app's executable names its .pdb without a path. Spelled out:
# MSBuild escapes the linker's own %_PDB% into a wrong name.
if(MSVC)
    foreach(app Showcase Demo Drift HelloAtom AtomArena)
        if(TARGET ${app}) # ATOM_BUILD_GAME=OFF (CI) defines only some of them
            target_link_options(${app} PRIVATE $<$<CONFIG:Release>:/PDBALTPATH:$<TARGET_PDB_FILE_NAME:${app}>>)
        endif()
    endforeach()
endif()

if(ATOM_BUILD_TOOLS OR ATOM_BUILD_WORKSPACE) # the workspace runs AtomLevelCheck (Validate)
    add_subdirectory(Tools/LevelCheck) # M112: AtomLevelCheck, levels checked without the game
    add_subdirectory(Tools/ProjectTool) # v0.0.19: AtomProjectTool create --workspace <folder> --name <Name>
endif()
if(ATOM_BUILD_WORKSPACE)
    add_subdirectory(Tools/Workspace) # v0.0.19: AtomWorkspace, the development program
endif()

if(ATOM_BUILD_TESTS)
    add_subdirectory(Tests) # last: the aggregate AtomTests names every owner's
endif()

if(WIN32 AND ATOM_BUILD_PRESENTATION_PROBE)
    add_subdirectory(Tools/PresentationProbe)
endif()
