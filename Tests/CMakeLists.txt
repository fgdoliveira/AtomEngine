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
        Framework/EntityInspectorTests.cpp
        Framework/EnvironmentTests.cpp
        Framework/FakeGame.h
        Framework/FixedStepTests.cpp
        Framework/GameDiagnosticsTests.cpp
        Framework/GameSettingsTests.cpp
        Framework/ImpostorTests.cpp
        Framework/InteractionTests.cpp
        Framework/LabViewerTests.cpp
        Framework/LevelCheckTests.cpp
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
        Content/LevelCheckClipTests.cpp
    LIBS AtomFramework nlohmann_json::nlohmann_json
)

# AtomTests: every CPU test target, built together (CI, the full check).
get_property(ATOM_ALL_TESTS GLOBAL PROPERTY ATOM_TEST_TARGETS)
add_custom_target(AtomTests DEPENDS ${ATOM_ALL_TESTS})


# M113: the incremental export's guarantees (Tools/Blender/test_export.ps1),
# against the real Blender build - registered only where Blender is
# installed (not CI). Minutes, not seconds: label "blender", never "cpu".
file(GLOB ATOM_BLENDER_INSTALLS "$ENV{ProgramFiles}/Blender Foundation/*/blender.exe")
find_program(ATOM_PWSH pwsh)
if(ATOM_PWSH AND (ATOM_BLENDER_INSTALLS OR DEFINED ENV{ATOM_BLENDER}))
    add_test(NAME Blender.export COMMAND ${ATOM_PWSH} -NoProfile -File "${CMAKE_SOURCE_DIR}/Tools/Blender/test_export.ps1")
    set_tests_properties(Blender.export PROPERTIES LABELS blender TIMEOUT 1800)
    # M115: the AtomEngine panel's markers export exactly like the builders'.
    if(DEFINED ENV{ATOM_BLENDER})
        set(ATOM_BLENDER_EXE "$ENV{ATOM_BLENDER}")
    else()
        list(SORT ATOM_BLENDER_INSTALLS)
        list(GET ATOM_BLENDER_INSTALLS -1 ATOM_BLENDER_EXE) # the newest
    endif()
    add_test(NAME Blender.markers
             COMMAND ${ATOM_BLENDER_EXE} -b --factory-startup --python "${CMAKE_SOURCE_DIR}/Tools/Blender/test_markers.py")
    set_tests_properties(Blender.markers PROPERTIES LABELS blender TIMEOUT 300)
endif()
