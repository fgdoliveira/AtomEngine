# ATOM ARENA (v0.0.15, M93): a survival-arena benchmark sample, built to
# evaluate CPU profiling (ADR-010). ArenaLib, the pure
# simulation, is what the unit tests reach (built without the game too, as
# CI does); the executable is HelloAtom's shape.
add_library(ArenaLib STATIC ArenaSim.cpp ArenaSim.h ArenaBenchmark.cpp ArenaBenchmark.h)
target_include_directories(ArenaLib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ArenaLib PUBLIC glm::glm AtomProfile) # M95: its zones

# ATOM ARENA's CPU tests (M105): the simulation, its level.
if(ATOM_BUILD_TESTS)
    atom_add_tests(
        ArenaTests
        OWNER arena
        ASSETS "${CMAKE_CURRENT_SOURCE_DIR}/Assets"
        SOURCES
            Tests/ArenaSimTests.cpp
            Tests/LevelContentTests.cpp
        LIBS ArenaLib AtomFramework
    )
endif()

add_executable(AtomArena Main.cpp ArenaApp.cpp ArenaApp.h)
target_link_libraries(AtomArena PRIVATE ArenaLib AtomEngine AtomFramework imgui)
# M94: what a benchmark report names - the version, the commit (when CMake
# last configured; Tools/Perf/arena.ps1 also records HEAD) and the build.
execute_process(COMMAND git rev-parse --short HEAD WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                OUTPUT_VARIABLE ARENA_GIT_COMMIT OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
if(NOT ARENA_GIT_COMMIT)
    set(ARENA_GIT_COMMIT "unknown")
endif()
target_compile_definitions(AtomArena PRIVATE ATOM_VERSION="${PROJECT_VERSION}" ATOM_GIT_COMMIT="${ARENA_GIT_COMMIT}"
                                             ATOM_BUILD_CONFIG="$<CONFIG>")
if(TARGET AtomShaders)
    add_dependencies(AtomArena AtomShaders) # the engine's shaders, shared with every app
endif()

# Its assets beside the executable: its own in Assets/AtomArena/ (looked up
# first), the shared pieces it uses from Content/ in Assets/<folder>.
set(ARENA_COPY_COMMANDS
    COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/Assets $<TARGET_FILE_DIR:AtomArena>/Assets/AtomArena)
foreach(folder IN ITEMS First Fonts Kit)
    list(APPEND ARENA_COPY_COMMANDS
        COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
            ${CMAKE_SOURCE_DIR}/Content/${folder} $<TARGET_FILE_DIR:AtomArena>/Assets/${folder})
endforeach()
add_custom_command(TARGET AtomArena POST_BUILD ${ARENA_COPY_COMMANDS} COMMENT "Copying ATOM ARENA's assets" VERBATIM)
