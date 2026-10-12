# CPU tests (no window, no GPU), one executable per owner (v0.0.16, M105;
# cmake/AtomTesting.cmake). Here: the engine's, the framework's and the
# shared content's. Each app's are in its own folder (<app>/Tests).
#
# The engine's: its algorithms on synthetic inputs - nothing but AtomEngine.
atom_add_tests(
    AtomEngineTests
    OWNER engine
    SOURCES
        Engine/AnimationTests.cpp
        Engine/CollisionGridTests.cpp
        Engine/CrashReportTests.cpp
        Engine/FrameStatsTests.cpp
        Engine/GPUDeviceTests.cpp
        Engine/GpuResourcesTests.cpp
        Engine/LatencyProbeTests.cpp
        Engine/LightingTests.cpp
        Engine/ModelTests.cpp
        Engine/RaycastTests.cpp
        Engine/ReverbTests.cpp
        Engine/SlotMapTests.cpp
        Engine/SynthTests.cpp
        Engine/TraceTests.cpp
    LIBS AtomEngine
)

# The framework's: its reusable policies and parsers, on data written in
# the test.
atom_add_tests(
    AtomFrameworkTests
    OWNER framework
    SOURCES
        Framework/AnimatorTests.cpp
        Framework/AuthoringTests.cpp
        Framework/BasicTestHooksTests.cpp
        Framework/CalibrationTests.cpp
        Framework/DriveTests.cpp
        Framework/EnvironmentControllerTests.cpp
        Framework/EnvironmentTests.cpp
        Framework/FakeGame.h
        Framework/FixedStepTests.cpp
        Framework/GameDiagnosticsTests.cpp
        Framework/GameSettingsTests.cpp
        Framework/ImpostorTests.cpp
        Framework/InteractionTests.cpp
        Framework/LabViewerTests.cpp
        Framework/LevelTests.cpp
        Framework/LogTests.cpp
        Framework/NightStreetTests.cpp
        Framework/PairedBenchTests.cpp
        Framework/RunLogTests.cpp
        Framework/SequenceTests.cpp
        Framework/TestScriptTests.cpp
        Framework/UiKitTests.cpp
    LIBS AtomFramework
)

# The shared content's (Content/): the kit, the environment presets.
atom_add_tests(
    AtomContentTests
    OWNER content
    SOURCES
        Content/EnvironmentPresetTests.cpp
        Content/KitAnimationTests.cpp
        Content/KitModelTests.cpp
    LIBS AtomFramework nlohmann_json::nlohmann_json
)

# AtomTests: every CPU test target, built together (CI, the full check).
get_property(ATOM_ALL_TESTS GLOBAL PROPERTY ATOM_TEST_TARGETS)
add_custom_target(AtomTests DEPENDS ${ATOM_ALL_TESTS})

