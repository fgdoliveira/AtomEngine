# A game outside the engine (v0.0.20, M127): its own top-level CMake, the
# engine added below it. Copied to a scratch folder and built by
# ConsumerBuilds.cmake. It sets no C++ standard, no runtime library and no
# engine define of its own: all of that must arrive with the engine's
# targets, or this fails to compile (C++20 headers), to link (LNK2038) or to
# agree on ATOM_DEV_TOOLS (Main.cpp's check).
cmake_minimum_required(VERSION 3.26)
project(SdkConsumer LANGUAGES CXX)

set(ATOM_ENGINE_DIR "" CACHE PATH "The AtomEngine source folder")
if(NOT EXISTS "${ATOM_ENGINE_DIR}/Engine/Core/Application.h")
    message(FATAL_ERROR "ATOM_ENGINE_DIR must name the AtomEngine folder (got '${ATOM_ENGINE_DIR}')")
endif()
add_subdirectory("${ATOM_ENGINE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/atom")

add_executable(SdkConsumer Main.cpp)
target_link_libraries(SdkConsumer PRIVATE AtomEngine AtomFramework)
atom_app_payload(SdkConsumer) # its folder: SDL3.dll, the shaders, the runtime library

# Under a game the engine builds only what a game needs.
foreach(unwanted Showcase Demo Drift HelloAtom AtomArena AtomWorkspace AtomLevelCheck AtomProjectTool AtomEngineTests imgui)
    if(TARGET ${unwanted})
        message(FATAL_ERROR "the engine added '${unwanted}' under a game - it should build only what a game needs")
    endif()
endforeach()
