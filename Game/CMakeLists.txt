add_executable(
    AtomGame
    Main.cpp
    Atmosphere.cpp
    Atmosphere.h
    AudioScape.cpp
    AudioScape.h
    SoundSynth.cpp
    SoundSynth.h
    DemoApp.cpp
    DemoApp.h
    PlayerController.cpp
    PlayerController.h
)

target_link_libraries(
    AtomGame
    PRIVATE
        AtomEngine
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