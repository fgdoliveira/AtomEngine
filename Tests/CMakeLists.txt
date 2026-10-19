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
        Engine/CameraTests.cpp
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
        Framework/AssetRootsTests.cpp
        Framework/LevelTests.cpp
        Framework/SaveGameTests.cpp
        Framework/LogTests.cpp
        Framework/NightStreetTests.cpp
        Framework/PairedBenchTests.cpp
        Framework/RunLogTests.cpp
        Framework/SequenceTests.cpp
        Framework/TestScriptTests.cpp
        Framework/UiKitTests.cpp
    LIBS AtomAuthoring # v0.0.19: the framework, plus LevelCheck and the inspector (EntityInspectorTests, LevelCheckTests)
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
    LIBS AtomAuthoring nlohmann_json::nlohmann_json
)

# The project service's (v0.0.19): descriptors, names, creation. No GPU,
# no engine - it runs anywhere CI does.
atom_add_tests(
    AtomProjectTests
    OWNER project
    SOURCES
        Project/ProjectTests.cpp
    LIBS AtomProject
)
# M121: a fresh project configures, is discovered, and builds nothing yet.
# Configures the whole engine once (about a minute): label "configure", not
# "cpu", so CI's quick run doesn't carry it; the full check does.
add_test(NAME Project.empty_configures
         COMMAND ${CMAKE_COMMAND} -DENGINE=${ATOM_ENGINE_ROOT} -DTOOL=$<TARGET_FILE:AtomProjectTool>
                 -DWORK=${CMAKE_BINARY_DIR}/empty-project-test -P ${CMAKE_CURRENT_SOURCE_DIR}/Project/EmptyProjectConfigures.cmake)
set_tests_properties(Project.empty_configures PROPERTIES LABELS "configure;project" TIMEOUT 600)
set_property(GLOBAL APPEND PROPERTY ATOM_TEST_TARGETS AtomProjectTool)

# The workspace's (v0.0.19): what it keeps between sessions, without a window.
atom_add_tests(
    AtomWorkspaceTests
    OWNER workspace
    SOURCES
        Workspace/OperationsTests.cpp
        Workspace/RecentProjectsTests.cpp
    LIBS AtomWorkspaceUI
)
# The process tests run cmake -E (echo, false, sleep): always there in a build.
target_compile_definitions(AtomWorkspaceTests PRIVATE ATOM_TEST_CMAKE="${CMAKE_COMMAND}")
if(TARGET AtomWorkspace) # ATOM_BUILD_WORKSPACE=OFF builds the tests without it
    set_property(GLOBAL APPEND PROPERTY ATOM_TEST_TARGETS AtomWorkspace)
endif()

# v0.0.20 (M127): the engine builds under a game's own CMake - a consumer
# outside the repository, configured, built in Debug and Release and run. It
# builds SDL and the engine once more (minutes): label "configure", the full
# check's. CI runs the same script in its own job (M133, ci.yml sdk-consumer).
add_test(NAME SDK.consumer_builds
         COMMAND ${CMAKE_COMMAND} -DENGINE=${ATOM_ENGINE_ROOT} -DWORK=${CMAKE_BINARY_DIR}/sdk-consumer-test
                 -P ${CMAKE_CURRENT_SOURCE_DIR}/SDK/ConsumerBuilds.cmake)
set_tests_properties(SDK.consumer_builds PROPERTIES LABELS "configure;sdk" TIMEOUT 1800)
# M128: atom_add_project's descriptor rules, in script mode (seconds).
add_test(NAME SDK.read_project
         COMMAND ${CMAKE_COMMAND} -DENGINE=${ATOM_ENGINE_ROOT} -DWORK=${CMAKE_BINARY_DIR}/sdk-read-project-test
                 -P ${CMAKE_CURRENT_SOURCE_DIR}/SDK/ReadProject.cmake)
set_tests_properties(SDK.read_project PROPERTIES LABELS "cpu;sdk" TIMEOUT 120)

# AtomTests: every CPU test target, built together (CI, the full check).
get_property(ATOM_ALL_TESTS GLOBAL PROPERTY ATOM_TEST_TARGETS)
add_custom_target(AtomTests DEPENDS ${ATOM_ALL_TESTS})


# M113: the incremental export's guarantees (Tools/Blender/test_export.ps1),
# against the real Blender build - registered only where Blender is
# installed (not CI). Minutes, not seconds: label "blender", never "cpu".
file(GLOB ATOM_BLENDER_INSTALLS "$ENV{ProgramFiles}/Blender Foundation/*/blender.exe")
find_program(ATOM_PWSH pwsh)
if(ATOM_PWSH AND (ATOM_BLENDER_INSTALLS OR DEFINED ENV{ATOM_BLENDER}))
    add_test(NAME Blender.export COMMAND ${ATOM_PWSH} -NoProfile -File "${ATOM_ENGINE_ROOT}/Tools/Blender/test_export.ps1")
    set_tests_properties(Blender.export PROPERTIES LABELS blender TIMEOUT 1800)
    # M115: the AtomEngine panel's markers export exactly like the builders'.
    if(DEFINED ENV{ATOM_BLENDER})
        set(ATOM_BLENDER_EXE "$ENV{ATOM_BLENDER}")
    else()
        list(SORT ATOM_BLENDER_INSTALLS)
        list(GET ATOM_BLENDER_INSTALLS -1 ATOM_BLENDER_EXE) # the newest
    endif()
    add_test(NAME Blender.markers
             COMMAND ${ATOM_BLENDER_EXE} -b --factory-startup --python "${ATOM_ENGINE_ROOT}/Tools/Blender/test_markers.py")
    set_tests_properties(Blender.markers PROPERTIES LABELS blender TIMEOUT 300)
endif()
