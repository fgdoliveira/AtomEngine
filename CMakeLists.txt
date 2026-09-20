cmake_minimum_required(VERSION 3.25)

project(
    AtomEngine
    VERSION 0.1.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# third party libraries
add_subdirectory(external/SDL)

add_subdirectory(Engine)
add_subdirectory(Game)

if(WIN32)
    add_subdirectory(Tools/PresentationProbe)
endif()
