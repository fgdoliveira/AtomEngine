# AtomWorkspace (v0.0.19): the development program - projects, and from
# M124 their levels previewed. Its own folder like an app's (SDL3.dll,
# the engine's shaders); it ships no assets of its own.
add_executable(AtomWorkspace Main.cpp WorkspaceApp.cpp WorkspaceApp.h WorkspacePanels.cpp WorkspaceOperations.cpp
                             WorkspaceSelfTest.cpp)
target_link_libraries(AtomWorkspace PRIVATE AtomWorkspaceUI)
target_compile_definitions(AtomWorkspace PRIVATE ATOM_VERSION="${PROJECT_VERSION}")
atom_app_payload(AtomWorkspace)

# M126: the self-test, in place of the games' F10 scenarios - open the
# Demo, preview, inspect, the sky picture, presets, create and open a
# project, Validate as a process. Smoke: an engine change can break it.
if(ATOM_BUILD_TESTS AND TARGET AtomShaders)
    atom_add_scenario(workspace_self_test OWNER workspace SMOKE TIMEOUT 240
        COMMAND $<TARGET_FILE:AtomWorkspace> --test "${CMAKE_BINARY_DIR}/workspace-test")
endif()
