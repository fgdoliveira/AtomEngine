# AtomWorkspace (v0.0.19): the development program - projects, and from
# M124 their levels previewed. Its own folder like an app's (SDL3.dll,
# the engine's shaders); it ships no assets of its own.
add_executable(AtomWorkspace Main.cpp WorkspaceApp.cpp WorkspaceApp.h WorkspacePanels.cpp)
target_link_libraries(AtomWorkspace PRIVATE AtomWorkspaceUI)
target_compile_definitions(AtomWorkspace PRIVATE ATOM_VERSION="${PROJECT_VERSION}")
atom_app_payload(AtomWorkspace)
