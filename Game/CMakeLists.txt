add_executable(
    AtomGame
    Main.cpp
)

target_link_libraries(
    AtomGame
    PRIVATE
        AtomEngine
)

set_property(
    DIRECTORY ${CMAKE_SOURCE_DIR}
    PROPERTY VS_STARTUP_PROJECT AtomGame
)