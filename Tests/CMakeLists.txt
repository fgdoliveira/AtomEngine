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
    GPUDeviceTests.cpp
    RunLogTests.cpp
    LatencyProbeTests.cpp
    DriftFlightTests.cpp
    DriftWorldTests.cpp
    SynthTests.cpp
    BoothSynthTests.cpp
    ShowcaseFeatureTests.cpp
    UiKitTests.cpp
    FakeGame.h
    TestAssets.h
)

target_link_libraries(
    AtomTests
    PRIVATE
        DemoLib
        DriftLib # M77: the second game's pure logic
        ShowcaseLib # v0.0.14: the synth booth
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
if(NOT TARGET Demo)
    return()
endif()

# In-game scenarios: each Tests/Scenarios/*.atomtest runs the real game with
# ATOM_TEST_SCRIPT set (needs a GPU and a window). `ctest -L scenario` runs
# them; `ctest -LE scenario` runs only the fast unit tests.
file(GLOB ATOM_SCENARIOS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Scenarios/*.atomtest")
foreach(scenario IN LISTS ATOM_SCENARIOS)
    get_filename_component(name "${scenario}" NAME_WE)
    add_test(NAME "Scenario.${name}" COMMAND $<TARGET_FILE:Demo>)
    set_tests_properties(
        "Scenario.${name}"
        PROPERTIES
            ENVIRONMENT "ATOM_TEST_SCRIPT=${scenario}"
            LABELS scenario
            # M53: a GPU wrapper outliving the renderer fails the scenario.
            FAIL_REGULAR_EXPRESSION "GPU resources still alive at shutdown;Failed to create [a-z ]*pipeline"
            TIMEOUT 240
    )
endforeach()

# v0.0.14: the Showcase's scenarios (Showcase/Scenarios/*.atomtest) run the
# Showcase the same way.
file(GLOB ATOM_SHOWCASE_SCENARIOS CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/Showcase/Scenarios/*.atomtest")
foreach(scenario IN LISTS ATOM_SHOWCASE_SCENARIOS)
    get_filename_component(name "${scenario}" NAME_WE)
    add_test(NAME "Scenario.${name}" COMMAND $<TARGET_FILE:Showcase>)
    set_tests_properties(
        "Scenario.${name}"
        PROPERTIES
            ENVIRONMENT "ATOM_TEST_SCRIPT=${scenario}"
            LABELS scenario
            FAIL_REGULAR_EXPRESSION "GPU resources still alive at shutdown;Failed to create [a-z ]*pipeline"
            TIMEOUT 240
    )
endforeach()

# M62: --diagnostics starts the engine, writes its report and exits (a GPU
# run like the scenarios, so it carries their label).
add_test(
    NAME Scenario.diagnostics
    COMMAND $<TARGET_FILE:Demo> --no-settings --diagnostics "${CMAKE_CURRENT_BINARY_DIR}/diagnostics.txt"
)
# M64: a short calibration runs to its end and reports - which tier it picks
# depends on the machine, so only completion is checked. On battery it
# refuses, which is also a correct end.
add_test(
    NAME Scenario.calibration
    COMMAND $<TARGET_FILE:Demo> --calibrate --no-settings
)
set_tests_properties(
    Scenario.calibration
    PROPERTIES
        LABELS scenario
        ENVIRONMENT "ATOM_CALIBRATE_SECONDS=0.5"
        PASS_REGULAR_EXPRESSION "Calibration: (high|balanced|low|no result|refused) - "
        FAIL_REGULAR_EXPRESSION "GPU resources still alive at shutdown;Failed to create [a-z ]*pipeline"
        TIMEOUT 180
)

# A high-performance request reaches a working device, directly or by the
# fallback, and the report says which. Whatever this machine's GPUs do, so
# only completion and clean shutdown are checked.
add_test(
    NAME Scenario.gpu_fallback
    COMMAND $<TARGET_FILE:Demo> --gpu high-performance --no-settings --diagnostics "${CMAKE_CURRENT_BINARY_DIR}/diagnostics_hp.txt"
)
set_tests_properties(
    Scenario.gpu_fallback
    PROPERTIES
        LABELS scenario
        PASS_REGULAR_EXPRESSION "Diagnostics written"
        FAIL_REGULAR_EXPRESSION "GPU resources still alive at shutdown;Failed to create [a-z ]*pipeline"
        TIMEOUT 120
)

set_tests_properties(
    Scenario.diagnostics
    PROPERTIES
        LABELS scenario
        PASS_REGULAR_EXPRESSION "Diagnostics written"
        FAIL_REGULAR_EXPRESSION "GPU resources still alive at shutdown;Failed to create [a-z ]*pipeline"
        TIMEOUT 120
)

# M77: DRIFT flies on autopilot for a few seconds and reports the distance:
# the second game starts, loads its assets, renders and shuts down clean.
if(TARGET Drift)
    add_test(NAME Scenario.drift_fly COMMAND $<TARGET_FILE:Drift>)
    set_tests_properties(
        Scenario.drift_fly
        PROPERTIES
            LABELS scenario
            ENVIRONMENT "ATOM_DRIFT_SECONDS=20;ATOM_DRIFT_SEED=7"
            PASS_REGULAR_EXPRESSION "DRIFT autopilot: flew [0-9]+ m .* rings [1-9][0-9]* passed"
            FAIL_REGULAR_EXPRESSION "GPU resources still alive at shutdown;Failed to create [a-z ]*pipeline"
            TIMEOUT 120
    )
    # M82: DRIFT's --diagnostics, the framework's shared report.
    add_test(
        NAME Scenario.drift_diagnostics
        COMMAND $<TARGET_FILE:Drift> --no-settings --diagnostics "${CMAKE_CURRENT_BINARY_DIR}/drift_diagnostics.txt"
    )
    set_tests_properties(
        Scenario.drift_diagnostics
        PROPERTIES
            LABELS scenario
            PASS_REGULAR_EXPRESSION "Diagnostics written"
            FAIL_REGULAR_EXPRESSION "GPU resources still alive at shutdown;Failed to create [a-z ]*pipeline"
            TIMEOUT 120
    )
endif()
