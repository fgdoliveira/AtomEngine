# The game framework (M76): what every app made with AtomEngine shares and
# the engine itself doesn't own - the run's log, the settings model and
# command line, the calibration decision, diagnostics and, since v0.0.14
# (M85), the world layer: data-driven levels, the entity world,
# interaction, environments, characters and the scenario harness. Apps
# link it; they don't share each other's code, and it names none of them:
# what is an app's own (sounds, screen programs, dialogue) comes in through
# interfaces (SoundLibrary, ScreenFactory, the ActionContext callbacks).
# Namespace AtomFramework.
add_library(
    AtomFramework
    STATIC
    Platform/RunLog.cpp
    Platform/RunLog.h
    Platform/AssetRoots.cpp
    Platform/AssetRoots.h
    Settings/GameSettings.cpp
    Settings/GameSettings.h
    Settings/SettingsStore.cpp
    Settings/SettingsStore.h
    Settings/Calibration.cpp
    Settings/Calibration.h
    Diagnostics/DiagnosticsReport.cpp
    Diagnostics/DiagnosticsReport.h
    # v0.0.14 (M89): the F10 panels every app shares.
    Debug/DevPanels.cpp
    Debug/DevPanels.h
    # v0.0.14 (M89): the player UI every app shares.
    UI/UiFonts.cpp
    UI/UiFonts.h
    UI/UiKit.cpp
    UI/UiKit.h
    # M90: the title/pause menu and the player's settings screen.
    UI/MenuScreen.cpp
    UI/MenuScreen.h
    UI/SettingsScreen.cpp
    UI/SettingsScreen.h
    # v0.0.14 (M85): the world layer, moved from the demo - levels, the
    # world, interaction, environments, characters, the scenario harness.
    Character/Animator.cpp
    Character/Animator.h
    Character/LabViewer.cpp
    Character/LabViewer.h
    Character/PlayerController.cpp
    Character/PlayerController.h
    Character/SpringArm.cpp
    Character/SpringArm.h
    Environment/EnvironmentController.cpp
    Environment/Atmosphere.cpp
    Environment/Atmosphere.h
    Environment/EnvironmentController.h
    Environment/EnvironmentPresets.cpp
    Environment/EnvironmentPresets.h
    Interaction/ActionExecutor.cpp
    Interaction/ActionExecutor.h
    Interaction/Actions.h
    Interaction/InteractionSystem.cpp
    Interaction/InteractionSystem.h
    Interaction/MessageFeed.cpp
    Interaction/MessageFeed.h
    Interaction/Sequence.cpp
    Interaction/Sequence.h
    Level/Environment.cpp
    Level/Environment.h
    Level/FileWatcher.h
    Level/JsonText.h
    Level/Level.cpp
    Level/Level.h
    Level/LevelData.cpp
    Level/LevelData.h
    Level/LevelManager.cpp
    Level/LevelManager.h
    Level/ModelCache.h
    Level/ScreenProgram.h
    Level/SoundLibrary.h
    Level/ViewToggles.cpp
    Level/ViewToggles.h
    Testing/GameDiagnostics.cpp
    Testing/GameDiagnostics.h
    Testing/PairedBench.cpp
    Testing/PairedBench.h
    Testing/TestScript.cpp
    Testing/TestScript.h
    World/FixedStep.h
    World/GameState.h
    World/GameWorld.h
    World/Impostors.cpp
    World/Impostors.h
    World/LiveEffects.cpp
    World/LiveEffects.h
)

target_include_directories(
    AtomFramework
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# SDL for the pref path (RunLog); JSON only inside the parsers (settings, levels).
target_link_libraries(
    AtomFramework
    PUBLIC
        SDL3::SDL3
        AtomEngine # M82: the developer switches, and the renderer the diagnostics report reads
    PRIVATE
        nlohmann_json::nlohmann_json
        imgui # M89: only inside DevPanels.cpp
)
