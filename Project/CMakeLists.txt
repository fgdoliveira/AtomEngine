# AtomProject (v0.0.19): what a project is and how one is made - the
# descriptor (<Name>.atomproject.json), names and paths checked, creation,
# discovery. No GPU, no ImGui, no engine: the command-line tool, the
# workspace and the tests all use it, and it runs anywhere.
add_library(AtomProject STATIC)

target_sources(
    AtomProject
    PRIVATE
        Project/ProjectCreator.cpp
        Project/ProjectCreator.h
        Project/ProjectDescriptor.cpp
        Project/ProjectDescriptor.h
        Project/ProjectDiscovery.cpp
        Project/ProjectDiscovery.h
)

target_include_directories(
    AtomProject
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(
    AtomProject
    PRIVATE
        nlohmann_json::nlohmann_json
)
