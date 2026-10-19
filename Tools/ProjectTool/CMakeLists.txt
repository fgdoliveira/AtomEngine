# AtomProjectTool (v0.0.19): create a project from the command line - the
# same service the workspace uses (AtomProject). No window, no GPU. v0.0.20:
# it knows the engine it was built from, for the project's user presets
# (compiled into the tool only; never written into a tracked file).
add_executable(AtomProjectTool Main.cpp)
target_link_libraries(AtomProjectTool PRIVATE AtomProject)
target_compile_definitions(AtomProjectTool PRIVATE ATOM_VERSION="${PROJECT_VERSION}" ATOM_ENGINE_ROOT_DIR="${ATOM_ENGINE_ROOT}")
