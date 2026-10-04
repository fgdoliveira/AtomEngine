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
    CollisionGridTests.cpp
    ImpostorTests.cpp
    NightStreetTests.cpp
    SequenceTests.cpp
    PachinkoTests.cpp
    MachineModeTests.cpp
    Physics2DTests.cpp
    PlayfieldTests.cpp
    PachinkoRulesTests.cpp
    SkinningTests.cpp
    LabViewerTests.cpp
    AnimatorTests.cpp
    DriveTests.cpp
    LightingTests.cpp
    FlashlightTests.cpp
    FrameStatsTests.cpp
    GpuResourcesTests.cpp
    EnvironmentTests.cpp
    EnvironmentControllerTests.cpp
    PairedBenchTests.cpp
    GameDiagnosticsTests.cpp
    GameSettingsTests.cpp
    CalibrationTests.cpp
    FakeGame.h
)

target_link_libraries(
    AtomTests
    PRIVATE
        AtomGameLib
        nlohmann_json::nlohmann_json # the authoring tests read JSON themselves
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

# In-game scenarios need the game (ATOM_BUILD_GAME).
if(NOT TARGET AtomGame)
    return()
endif()

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
            # M53: a GPU wrapper outliving the renderer fails the scenario.
            FAIL_REGULAR_EXPRESSION "GPU resources still alive at shutdown"
            TIMEOUT 240
    )
endforeach()

# M62: --diagnostics starts the engine, writes its report and exits (a GPU
# run like the scenarios, so it carries their label).
add_test(
    NAME Scenario.diagnostics
    COMMAND $<TARGET_FILE:AtomGame> --no-settings --diagnostics "${CMAKE_CURRENT_BINARY_DIR}/diagnostics.txt"
)
# M64: a short calibration runs to its end and reports - which tier it picks
# depends on the machine, so only completion is checked.
add_test(
    NAME Scenario.calibration
    COMMAND $<TARGET_FILE:AtomGame> --calibrate --no-settings
)
set_tests_properties(
    Scenario.calibration
    PROPERTIES
        LABELS scenario
        ENVIRONMENT "ATOM_CALIBRATE_SECONDS=0.5"
        PASS_REGULAR_EXPRESSION "Calibration: (high|balanced|low|no result) - "
        FAIL_REGULAR_EXPRESSION "GPU resources still alive at shutdown"
        TIMEOUT 180
)

set_tests_properties(
    Scenario.diagnostics
    PROPERTIES
        LABELS scenario
        PASS_REGULAR_EXPRESSION "Diagnostics written"
        FAIL_REGULAR_EXPRESSION "GPU resources still alive at shutdown"
        TIMEOUT 120
)
