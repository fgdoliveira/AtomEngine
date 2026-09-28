# Unit tests for pure logic (no window, no GPU). Run with ctest or directly.
add_executable(
    AtomTests
    TestMain.cpp
    SlotMapTests.cpp
    RaycastTests.cpp
    InteractionTests.cpp
    DialogueTests.cpp
    LevelTests.cpp
    TestScriptTests.cpp
    ModelTests.cpp
    AnimationTests.cpp
    AuthoringTests.cpp
)

target_link_libraries(
    AtomTests
    PRIVATE
        AtomGameLib
)

target_include_directories(
    AtomTests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/external/doctest
)

add_test(NAME AtomTests COMMAND AtomTests)

# Lets tests validate shipped content (e.g. Assets/Dialogue/*.json).
target_compile_definitions(
    AtomTests
    PRIVATE
        ATOM_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

# In-game scenarios: each Tests/Scenarios/*.atomtest runs the real game with
# ATOM_TEST_SCRIPT set (needs a GPU and a window). `ctest -L scenario` runs
# them; `ctest -LE scenario` runs only the fast unit tests.
file(GLOB ATOM_SCENARIOS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Scenarios/*.atomtest")
foreach(scenario IN LISTS ATOM_SCENARIOS)
    get_filename_component(name "${scenario}" NAME_WE)
    add_test(NAME "Scenario.${name}" COMMAND $<TARGET_FILE:AtomGame>)
    set_tests_properties(
        "Scenario.${name}"
        PROPERTIES
            ENVIRONMENT "ATOM_TEST_SCRIPT=${scenario}"
            LABELS scenario
            TIMEOUT 240
    )
endforeach()
