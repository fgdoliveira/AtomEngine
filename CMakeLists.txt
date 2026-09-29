cmake_minimum_required(VERSION 3.25)

project(
    AtomEngine
    VERSION 0.0.4
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
