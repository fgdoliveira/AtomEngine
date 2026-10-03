# Gameplay code as a library so both the game and the unit tests use it.
add_library(
    AtomGameLib
    STATIC
    Atmosphere.cpp
    Atmosphere.h
    AudioScape.cpp
    AudioScape.h
    SoundSynth.cpp
    SoundSynth.h
    UneaseDirector.cpp
    UneaseDirector.h
    DemoApp.cpp
    DemoApp.h
    Flashlight.cpp
    Environment/EnvironmentController.cpp
    Environment/EnvironmentController.h
    Flashlight.h
    DemoAppLab.cpp
    DemoAppDevTools.cpp
    Character/Animator.cpp
    Character/Animator.h
    Character/LabViewer.cpp
    Character/SpringArm.cpp
    Character/SpringArm.h
    Character/LabViewer.h
    PlayerController.cpp
    PlayerController.h
    Testing/TestScript.cpp
    Testing/GameDiagnostics.cpp
    Testing/GameDiagnostics.h
    Testing/PairedBench.cpp
    Testing/PairedBench.h
    Testing/TestScript.h
    Dialogue/Dialogue.cpp
    Dialogue/Dialogue.h
    Level/FileWatcher.h
    Level/ModelCache.h
    Level/JsonText.h
    Dialogue/DialogueRunner.cpp
    Dialogue/DialogueRunner.h
    Dialogue/DialogueView.cpp
    Dialogue/DialogueView.h
    Interaction/Actions.h
    Level/Environment.cpp
    Level/Environment.h
    Level/Level.cpp
    Level/Level.h
    Level/LevelData.cpp
    Level/LevelData.h
    Level/LevelManager.cpp
    Level/LevelManager.h
    Interaction/ActionExecutor.cpp
    Interaction/ActionExecutor.h
    Interaction/InteractionSystem.cpp
    Interaction/Sequence.cpp
    Interaction/Sequence.h
    Interaction/InteractionSystem.h
    Interaction/MessageFeed.cpp
    Interaction/MessageFeed.h
    World/GameState.h
    World/GameWorld.h
    World/Impostors.cpp
    World/Impostors.h
    World/LiveEffects.cpp
    Input/InputContext.cpp
    Input/InputContext.h
    Pachinko/MachineMode.cpp
    Pachinko/MachineMode.h
    Pachinko/Physics2D.cpp
    Pachinko/Physics2D.h
    Pachinko/PhysicsDebugDraw.cpp
    Pachinko/PhysicsDebugDraw.h
    Pachinko/PachinkoGame.cpp
    Pachinko/PachinkoGame.h
    Pachinko/PachinkoRules.cpp
    Pachinko/PachinkoRules.h
    Pachinko/Playfield.cpp
    Pachinko/Playfield.h
    Pachinko/PixelDraw.h
    World/FixedStep.h
    World/PachinkoAttract.cpp
    World/PachinkoAttract.h
    World/LiveEffects.h
)

target_include_directories(
    AtomGameLib
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# JSON and ImGui are used only inside the library's .cpp files (the
# parsers, the developer panels), so they stay PRIVATE (M54).
target_link_libraries(
    AtomGameLib
    PUBLIC
        AtomEngine
    PRIVATE
        nlohmann_json::nlohmann_json
        imgui
)

target_compile_definitions(
    AtomGameLib
    PRIVATE
        ATOM_VERSION="${PROJECT_VERSION}" # the window title
)

if(NOT ATOM_BUILD_GAME)
    return() # the libraries only (ATOM_BUILD_GAME=OFF): no executable, no shaders
endif()

add_executable(
    AtomGame
    Main.cpp
)

target_link_libraries(
    AtomGame
    PRIVATE
        AtomGameLib
)

add_dependencies(AtomGame AtomShaders)

# The runtime asset payload (M56, audit ASSET-001): the folders of Assets/
# the game reads, copied next to the executable. Derived from evidence -
# every scenario and every level run with ATOM_ASSET_LOG opened 86 files
# from exactly these folders - and kept per folder, so a level that starts
# using another kit piece still finds it. Left out: Schemas/ (editors and
# the authoring tests only, read from the source tree). Fonts/ and
# ThirdParty/ also carry the licences and credits a distribution needs.
# A new folder the game reads goes here; every scenario runs against this
# copy, so a missing one fails them. ATOM_ASSET_ROOT (hot reload) reads
# the source tree itself.
set(ATOM_RUNTIME_ASSETS
    City Data Dialogue Environments Fields First Fonts Interior Kit Lab
    Lakeshore Levels Machines Night Pachinko Passage Shrine Sky Street
    ThirdParty
)
set(ATOM_ASSET_COPY_COMMANDS)
foreach(folder IN LISTS ATOM_RUNTIME_ASSETS)
    list(APPEND ATOM_ASSET_COPY_COMMANDS
        COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
            ${CMAKE_SOURCE_DIR}/Assets/${folder}
            $<TARGET_FILE_DIR:AtomGame>/Assets/${folder})
endforeach()
add_custom_command(
    TARGET AtomGame
    POST_BUILD
    ${ATOM_ASSET_COPY_COMMANDS}
    COMMENT "Copying the runtime assets"
    VERBATIM
)

set_property(
    DIRECTORY ${CMAKE_SOURCE_DIR}
    PROPERTY VS_STARTUP_PROJECT AtomGame
)
