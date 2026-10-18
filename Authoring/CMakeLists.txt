# AtomAuthoring (v0.0.19): what development tools know about content -
# levels checked without a game (LevelCheck), the entity inspection model,
# scene preview. It depends on the runtime; the runtime never depends on
# it, and no game links it: games ship only AtomEngine + AtomFramework.
# No ImGui here either - presentation is the workspace's (WorkspaceUI).
add_library(AtomAuthoring STATIC)

target_sources(
    AtomAuthoring
    PRIVATE
        Debug/EntityInspector.cpp
        Debug/EntityInspector.h
        Level/LevelCheck.cpp
        Level/LevelCheck.h
        Preview/ScenePreview.cpp
        Preview/ScenePreview.h
)

target_include_directories(
    AtomAuthoring
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(
    AtomAuthoring
    PUBLIC
        AtomFramework
)
