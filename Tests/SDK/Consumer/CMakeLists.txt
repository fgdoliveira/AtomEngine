# A game outside the engine (v0.0.20, M127-M128): its own top-level CMake,
# the engine added below it, its executable made from its descriptor by
# atom_add_project. Copied to a scratch folder and built by
# ConsumerBuilds.cmake. It sets no C++ standard, no runtime library and no
# engine define of its own: all of that must arrive with the engine's
# targets, or this fails to compile (C++20 headers), to link (LNK2038) or to
# agree on ATOM_DEV_TOOLS (Source/Main.cpp's check).
cmake_minimum_required(VERSION 3.26)
project(SdkConsumer LANGUAGES CXX)

set(ATOM_ENGINE_DIR "" CACHE PATH "The AtomEngine source folder")
if(NOT EXISTS "${ATOM_ENGINE_DIR}/Engine/Core/Application.h")
    message(FATAL_ERROR "ATOM_ENGINE_DIR must name the AtomEngine folder (got '${ATOM_ENGINE_DIR}')")
endif()
add_subdirectory("${ATOM_ENGINE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/atom")

# SdkConsumer.atomproject.json: the executable, its sources (Source/), its
# assets (Assets/ -> Assets/SdkConsumer/), plus the shared Kit.
atom_add_project(CONTENT Kit)
if(NOT ATOM_PROJECT_TARGET STREQUAL "SdkConsumer")
    message(FATAL_ERROR "atom_add_project made '${ATOM_PROJECT_TARGET}', not SdkConsumer")
endif()

# Under a game the engine builds only what a game needs.
foreach(unwanted Showcase Demo Drift HelloAtom AtomArena AtomWorkspace AtomLevelCheck AtomProjectTool AtomEngineTests imgui)
    if(TARGET ${unwanted})
        message(FATAL_ERROR "the engine added '${unwanted}' under a game - it should build only what a game needs")
    endif()
endforeach()
