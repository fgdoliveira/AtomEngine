# Gameplay code as a library so both the game and the unit tests use it.
add_library(
    DemoLib
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
    DemoAppCalibration.cpp
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
    Level/ScreenProgram.h
    Level/SoundLibrary.h
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
    Pachinko/PachinkoAttract.cpp
    Pachinko/PachinkoAttract.h
    Pachinko/LevelScreens.cpp
    Pachinko/LevelScreens.h
    World/LiveEffects.h
)

target_include_directories(
    DemoLib
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# JSON and ImGui are used only inside the library's .cpp files (the
# parsers, the developer panels), so they stay PRIVATE (M54).
target_link_libraries(
    DemoLib
    PUBLIC
        AtomEngine
        AtomFramework # M76: log, settings, calibration decision
    PRIVATE
        nlohmann_json::nlohmann_json
        imgui
)

target_compile_definitions(
    DemoLib
    PRIVATE
        ATOM_VERSION="${PROJECT_VERSION}" # the window title
)

if(NOT ATOM_BUILD_GAME)
    return() # the libraries only (ATOM_BUILD_GAME=OFF): no executable, no shaders
endif()

add_executable(
    Demo
    Main.cpp
)

target_link_libraries(
    Demo
    PRIVATE
        DemoLib
)

add_dependencies(Demo AtomShaders)

# M67: the distributed game is a windowed program - no console window on
# double-click. Development builds keep their console. Main.cpp keeps
# `main` either way (SDL_main.h supplies the Windows entry point); RunLog
# writes the log file in both and, when windowed, re-attaches to a
# terminal that started it.
if(ATOM_DISTRIBUTION)
    set_target_properties(Demo PROPERTIES WIN32_EXECUTABLE ON)
endif()

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
#
# M68: split in two. Shipped folders go into the distribution; the
# development-only ones (the character lab: its rigged character in
# ThirdParty/ has no recorded licence) stay in the build copy for the
# scenarios, and out of the package.
#
# v0.0.14: two source roots, one runtime folder. The demo's own art is in
# Games/Demo/Assets/; generic pieces other apps use too are in Content/
# (ATOM_CONTENT_ASSETS). Both land in Assets/<folder> beside the
# executable, so level files name them the same way.
set(ATOM_SHIPPED_ASSETS
    City Data Dialogue Environments Fields First Fonts Interior Kit
    Lakeshore Levels Machines Night Pachinko Passage Shrine Sky Street
)
set(ATOM_DEV_ASSETS Lab ThirdParty)
set(ATOM_CONTENT_ASSETS Fonts Kit Sky)
set(ATOM_RUNTIME_ASSETS ${ATOM_SHIPPED_ASSETS} ${ATOM_DEV_ASSETS})
function(atom_asset_source folder out)
    if(folder IN_LIST ATOM_CONTENT_ASSETS)
        set(${out} "${CMAKE_SOURCE_DIR}/Content/${folder}" PARENT_SCOPE)
    else()
        set(${out} "${CMAKE_CURRENT_SOURCE_DIR}/Assets/${folder}" PARENT_SCOPE)
    endif()
endfunction()
set(ATOM_ASSET_COPY_COMMANDS)
foreach(folder IN LISTS ATOM_RUNTIME_ASSETS)
    atom_asset_source(${folder} source)
    list(APPEND ATOM_ASSET_COPY_COMMANDS
        COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
            ${source}
            $<TARGET_FILE_DIR:Demo>/Assets/${folder})
endforeach()
add_custom_command(
    TARGET Demo
    POST_BUILD
    ${ATOM_ASSET_COPY_COMMANDS}
    COMMENT "Copying the runtime assets"
    VERBATIM
)

set_property(
    DIRECTORY ${CMAKE_SOURCE_DIR}
    PROPERTY VS_STARTUP_PROJECT Demo
)

# The distribution (M68): what `cmake --install` puts in a package folder
# - the one authoritative list. Tools/Dist/package.ps1 installs from a
# -DATOM_DISTRIBUTION=ON build, verifies and zips it. M76: every rule is in
# the Demo component, so each game installs (and packages) only its own
# files: `cmake --install ... --component Demo`.
install(TARGETS Demo RUNTIME DESTINATION . COMPONENT Demo)
install(FILES $<TARGET_FILE:SDL3::SDL3> DESTINATION . COMPONENT Demo) # the only runtime DLL (M66)
install(DIRECTORY "${ATOM_SHADER_OUTPUT_DIR}" DESTINATION . COMPONENT Demo) # Shaders/ exports the folder
foreach(folder IN LISTS ATOM_SHIPPED_ASSETS)
    atom_asset_source(${folder} source)
    install(
        DIRECTORY "${source}"
        DESTINATION Assets
        COMPONENT Demo
        PATTERN "character_lab*" EXCLUDE # the lab is development-only
    )
endforeach()

# Licences: AtomEngine's, and every third-party component compiled into
# the executable or shipped beside it (doctest is tests-only, not here).
set(ATOM_NOTICES "AtomEngine ${PROJECT_VERSION} - third-party notices\n\nDemo.exe and SDL3.dll contain the components below. Their licences require these notices to travel with the binaries.\n")
foreach(entry IN ITEMS
        "SDL 3|external/SDL/LICENSE.txt"
        "GLM|external/glm/copying.txt"
        "nlohmann/json|external/json/LICENSE.MIT"
        "cgltf|external/cgltf/LICENSE"
        "stb|external/stb/LICENSE"
        "Dear ImGui|external/imgui/LICENSE.txt"
        "Shippori Mincho (font)|Content/Fonts/OFL.txt")
    string(REPLACE "|" ";" parts "${entry}")
    list(GET parts 0 name)
    list(GET parts 1 file)
    file(READ "${CMAKE_SOURCE_DIR}/${file}" text)
    string(APPEND ATOM_NOTICES "\n==============================================================================\n${name}  (${file})\n==============================================================================\n\n${text}\n")
endforeach()
file(WRITE "${CMAKE_BINARY_DIR}/dist/THIRD_PARTY_NOTICES.txt" "${ATOM_NOTICES}")
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/README.txt.in" "${CMAKE_BINARY_DIR}/dist/README.txt" @ONLY)
install(
    FILES
        "${CMAKE_SOURCE_DIR}/LICENSE.txt"
        "${CMAKE_BINARY_DIR}/dist/THIRD_PARTY_NOTICES.txt"
        "${CMAKE_BINARY_DIR}/dist/README.txt"
    DESTINATION .
    COMPONENT Demo
)

# For Tools/Dist/verify.ps1: what this package must hold, from the lists
# above rather than a hand-kept copy. Removed before zipping. M76: it also
# names the game and its executable, so the scripts serve every game.
string(REPLACE ";" "," ATOM_SHIPPED_LIST "${ATOM_SHIPPED_ASSETS}")
string(REPLACE ";" "," ATOM_DEV_LIST "${ATOM_DEV_ASSETS}")
file(WRITE "${CMAKE_BINARY_DIR}/dist/payload.txt"
    "game=Demo\nexe=Demo.exe\nversion=${PROJECT_VERSION}\nshipped=${ATOM_SHIPPED_LIST}\ndevonly=${ATOM_DEV_LIST}\nshaders=${ATOM_SHADER_COUNT}\n")
install(FILES "${CMAKE_BINARY_DIR}/dist/payload.txt" DESTINATION . RENAME .payload COMPONENT Demo)
