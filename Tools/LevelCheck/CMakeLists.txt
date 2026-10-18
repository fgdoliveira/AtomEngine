# AtomLevelCheck (v0.0.17, M112): an app's level files checked without the
# game - no window, no GPU, so CI runs it too. The checks are the
# authoring library's (Authoring/Level/LevelCheck.h, v0.0.19).
add_executable(AtomLevelCheck Main.cpp)
target_link_libraries(AtomLevelCheck PRIVATE AtomAuthoring)
target_compile_definitions(AtomLevelCheck PRIVATE ATOM_SOURCE_DIR="${CMAKE_SOURCE_DIR}")

# Each app's levels, checked as a CPU test of that owner (the "content"
# kind: shipped files). Errors fail it; warnings are printed.
if(ATOM_BUILD_TESTS)
    set_property(GLOBAL APPEND PROPERTY ATOM_TEST_TARGETS AtomLevelCheck) # AtomTests builds it (CI)
    foreach(owner IN ITEMS showcase demo hello arena)
        add_test(NAME LevelCheck.${owner} COMMAND AtomLevelCheck --app ${owner})
        set_tests_properties(LevelCheck.${owner} PROPERTIES LABELS "cpu;content;${owner}")
    endforeach()
endif()
