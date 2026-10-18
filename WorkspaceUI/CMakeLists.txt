# AtomWorkspaceUI (v0.0.19): the workspace's presentation - the Dear ImGui
# host (docking, detachable panels), the recent projects, the operations'
# processes and what the Console makes of their output. The only library
# that links ImGui; the runtime is guarded against it (root CMakeLists.txt).
add_library(AtomWorkspaceUI STATIC)

target_sources(
    AtomWorkspaceUI
    PRIVATE
        ConsoleLines.cpp
        ConsoleLines.h
        ImGuiHost.cpp
        ImGuiHost.h
        ProcessRunner.cpp
        ProcessRunner.h
        RecentProjects.cpp
        RecentProjects.h
)

target_include_directories(
    AtomWorkspaceUI
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(
    AtomWorkspaceUI
    PUBLIC
        imgui
        AtomAuthoring
        AtomProject
)
