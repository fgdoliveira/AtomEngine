cmake_minimum_required(VERSION 3.25)

project(
    AtomEngine
    VERSION 0.0.7
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

option(
    ATOM_BUILD_PRESENTATION_PROBE
    "Build the private D3D12 presentation investigation probe"
    OFF
)

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

add_subdirectory(Shaders)
add_subdirectory(Engine)
add_subdirectory(Game)

option(ATOM_BUILD_TESTS "Build the unit tests (AtomTests)" ON)
if(ATOM_BUILD_TESTS)
    enable_testing()
    add_subdirectory(Tests)
endif()

if(WIN32 AND ATOM_BUILD_PRESENTATION_PROBE)
    add_subdirectory(Tools/PresentationProbe)
endif()
