# The Showcase (v0.0.14): every engine feature in one scene, built only from
# the engine and the framework - the engine's front door, and the default
# startup project. A showcase, not a product: it has no install rules and
# is never packaged (the pavilion's character has no recorded licence).

# What the unit tests reach (built without the game too, as CI does): the
# synth booth's sequencer.
add_library(
    ShowcaseLib
    STATIC
    BoothSynth.cpp
    BoothSynth.h
    Features/Features.cpp
    Features/Features.h
)
target_include_directories(ShowcaseLib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ShowcaseLib PUBLIC AtomEngine)

if(NOT ATOM_BUILD_GAME)
    return() # the libraries only: no executable, no shaders
endif()

add_executable(
    Showcase
    Main.cpp
    ShowcaseApp.cpp
    ShowcaseApp.h
    ShowcaseDevTools.cpp
    ShowcaseLab.cpp
    ShowcaseLens.cpp
    ShowcaseMenus.cpp
)
target_link_libraries(
    Showcase
    PRIVATE
        ShowcaseLib
        AtomEngine
        AtomFramework
        imgui # M89: its own F10 panels (ShowcaseDevTools.cpp)
)
target_compile_definitions(Showcase PRIVATE ATOM_VERSION="${PROJECT_VERSION}") # M90: the benchmark file
add_dependencies(Showcase AtomShaders) # the engine's shaders, shared with every app
if(ATOM_DISTRIBUTION)
    set_target_properties(Showcase PROPERTIES WIN32_EXECUTABLE ON)
endif()

# Its assets beside the executable: the shared pieces (Content/) in
# Assets/<folder>, as every app has them, and its own in Assets/Showcase/,
# looked up first.
set(SHOWCASE_COPY_COMMANDS
    COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/Assets $<TARGET_FILE_DIR:Showcase>/Assets/Showcase)
foreach(folder IN ITEMS Environments First Fonts Kit Sky)
    list(APPEND SHOWCASE_COPY_COMMANDS
        COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
            ${CMAKE_SOURCE_DIR}/Content/${folder} $<TARGET_FILE_DIR:Showcase>/Assets/${folder})
endforeach()
add_custom_command(
    TARGET Showcase
    POST_BUILD
    ${SHOWCASE_COPY_COMMANDS}
    COMMENT "Copying the Showcase's assets"
    VERBATIM
)

set_property(DIRECTORY ${CMAKE_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT Showcase)
