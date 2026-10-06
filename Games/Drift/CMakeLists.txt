# DRIFT (M77-): a second game made with AtomEngine, ported from a three.js
# original. It uses the engine and the framework; it shares no code with
# the demo (Game/).

# The game's pure logic, unit-tested (built without the game too, as CI does).
add_library(
    DriftLib
    STATIC
    Flight.cpp
    Flight.h
    World.cpp
    World.h
)
target_include_directories(DriftLib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(DriftLib PUBLIC glm::glm)

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
