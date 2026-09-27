add_executable(
    AtomGame
    Main.cpp
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

set_property(
    DIRECTORY ${CMAKE_SOURCE_DIR}
    PROPERTY VS_STARTUP_PROJECT AtomGame
)