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
    PlayerController.cpp
    PlayerController.h
    Testing/TestScript.cpp
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
    Level/Level.cpp
    Level/Level.h
    Level/LevelData.cpp
    Level/LevelData.h
    Level/LevelManager.cpp
    Level/LevelManager.h
    Interaction/ActionExecutor.cpp
    Interaction/ActionExecutor.h
    Interaction/InteractionSystem.cpp
    Interaction/InteractionSystem.h
    Interaction/MessageFeed.cpp
    Interaction/MessageFeed.h
    World/GameState.h
    World/GameWorld.h
    World/Impostors.cpp
    World/Impostors.h
    World/LiveEffects.cpp
    World/LiveEffects.h
)

target_include_directories(
    AtomGameLib
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(
    AtomGameLib
    PUBLIC
        AtomEngine
        nlohmann_json::nlohmann_json
)

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

# Ship the asset folder next to the executable.
add_custom_command(
    TARGET AtomGame
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/Assets
        $<TARGET_FILE_DIR:AtomGame>/Assets
    COMMENT "Copying assets"
    VERBATIM
)

set_property(
    DIRECTORY ${CMAKE_SOURCE_DIR}
    PROPERTY VS_STARTUP_PROJECT AtomGame
)
