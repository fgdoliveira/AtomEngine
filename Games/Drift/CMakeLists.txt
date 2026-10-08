# DRIFT (M77-): a second game made with AtomEngine, ported from a three.js
# original. It uses the engine and the framework; it shares no code with
# the demo (Games/Demo/).

# The game's pure logic, unit-tested (built without the game too, as CI does).
add_library(
    DriftLib
    STATIC
    Flight.cpp
    Flight.h
    World.cpp
    World.h
    SpeedField.cpp
    SpeedField.h
    Music.cpp
    Music.h
)
target_include_directories(DriftLib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(DriftLib PUBLIC glm::glm AtomEngine) # M80: the engine's Synth

if(NOT ATOM_BUILD_GAME)
    return() # the libraries only: no executable, no shaders
endif()

add_executable(
    Drift
    Main.cpp
    DriftApp.cpp
    DriftApp.h
)
target_link_libraries(
    Drift
    PRIVATE
        DriftLib
        AtomEngine
        AtomFramework
)
target_compile_definitions(Drift PRIVATE ATOM_VERSION="${PROJECT_VERSION}") # M82: the diagnostics title
add_dependencies(Drift AtomShaders) # the engine's shaders, shared with every game
if(ATOM_DISTRIBUTION)
    set_target_properties(Drift PROPERTIES WIN32_EXECUTABLE ON) # M67: no console for players
endif()

# Its assets, beside the executable under Assets/Drift/ (the demo's are
# under Assets/<folder>; both games share the build's bin folder).
add_custom_command(
    TARGET Drift
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/Assets
        $<TARGET_FILE_DIR:Drift>/Assets/Drift
    COMMENT "Copying DRIFT's assets"
    VERBATIM
)

# The distribution (M83): DRIFT's own install component, as the demo's
# (Games/Demo/CMakeLists.txt). `pwsh Tools/Dist/package.ps1 -Game Drift`.
install(TARGETS Drift RUNTIME DESTINATION . COMPONENT Drift)
install(FILES $<TARGET_FILE:SDL3::SDL3> DESTINATION . COMPONENT Drift)
install(DIRECTORY "${ATOM_SHADER_OUTPUT_DIR}" DESTINATION . COMPONENT Drift) # the engine's shaders
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/Assets/" DESTINATION Assets/Drift COMPONENT Drift)

# Licences: AtomEngine's and every third-party component in Drift.exe and
# SDL3.dll (ImGui links but never starts in a package; its code is there).
set(DRIFT_NOTICES "DRIFT ${PROJECT_VERSION} (AtomEngine) - third-party notices\n\nDrift.exe and SDL3.dll contain the components below. Their licences require these notices to travel with the binaries.\n")
foreach(entry IN ITEMS
        "SDL 3|external/SDL/LICENSE.txt"
        "GLM|external/glm/copying.txt"
        "nlohmann/json|external/json/LICENSE.MIT"
        "cgltf|external/cgltf/LICENSE"
        "stb|external/stb/LICENSE"
        "Dear ImGui|external/imgui/LICENSE.txt"
        "Space Grotesk (font)|Games/Drift/Assets/Fonts/OFL.txt")
    string(REPLACE "|" ";" parts "${entry}")
    list(GET parts 0 name)
    list(GET parts 1 file)
    file(READ "${CMAKE_SOURCE_DIR}/${file}" text)
    string(APPEND DRIFT_NOTICES "\n==============================================================================\n${name}  (${file})\n==============================================================================\n\n${text}\n")
endforeach()
file(WRITE "${CMAKE_BINARY_DIR}/dist-drift/THIRD_PARTY_NOTICES.txt" "${DRIFT_NOTICES}")
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/README.txt.in" "${CMAKE_BINARY_DIR}/dist-drift/README.txt" @ONLY)
install(
    FILES
        "${CMAKE_SOURCE_DIR}/LICENSE.txt"
        "${CMAKE_BINARY_DIR}/dist-drift/THIRD_PARTY_NOTICES.txt"
        "${CMAKE_BINARY_DIR}/dist-drift/README.txt"
    DESTINATION .
    COMPONENT Drift
)

# For verify.ps1 (removed before zipping): the game, its executable and
# what it must hold.
file(WRITE "${CMAKE_BINARY_DIR}/dist-drift/payload.txt"
    "game=Drift\nexe=Drift.exe\nversion=${PROJECT_VERSION}\nshipped=Drift\ndevonly=\nshaders=${ATOM_SHADER_COUNT}\n")
install(FILES "${CMAKE_BINARY_DIR}/dist-drift/payload.txt" DESTINATION . RENAME .payload COMPONENT Drift)
