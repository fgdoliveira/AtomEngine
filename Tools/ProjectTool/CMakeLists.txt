# AtomProjectTool (v0.0.19): create a project from the command line - the
# same service the workspace uses (AtomProject). No window, no GPU.
add_executable(AtomProjectTool Main.cpp)
target_link_libraries(AtomProjectTool PRIVATE AtomProject)
target_compile_definitions(AtomProjectTool PRIVATE ATOM_VERSION="${PROJECT_VERSION}")
