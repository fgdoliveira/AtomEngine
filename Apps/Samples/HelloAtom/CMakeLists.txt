# HelloAtom (v0.0.14, M91): the smallest app on the engine and the framework
# - see docs/Getting-Started.md. Built in every configuration, CI included
# (it needs no shader compiler to compile); its shaders come with the game
# build, which running it needs.
add_executable(HelloAtom Main.cpp)
target_link_libraries(HelloAtom PRIVATE AtomEngine AtomFramework)

# Its own folder, bin/<config>/HelloAtom/ (M108, cmake/AtomApps.cmake):
# SDL3.dll, the engine's shaders (when built), its assets in
# Assets/HelloAtom/ (looked up first) and the shared pieces it uses from
# Content/ in Assets/<folder>.
atom_app_payload(HelloAtom COPY
    "${CMAKE_CURRENT_SOURCE_DIR}/Assets" "Assets/HelloAtom"
    "${CMAKE_SOURCE_DIR}/Content/First" "Assets/First"
    "${CMAKE_SOURCE_DIR}/Content/Kit" "Assets/Kit")

# Its scenario (M107, cmake/AtomTesting.cmake) - registered only when the
# shaders that running it needs are built (not in CI).
if(ATOM_BUILD_TESTS AND TARGET AtomShaders)
    atom_add_scenarios(HelloAtom OWNER hello DIR "${CMAKE_CURRENT_SOURCE_DIR}/Scenarios" SMOKE hello_atom TIMEOUT 120)
    # M116: the hand-made room, started in it.
    atom_add_scenario(hello_room OWNER hello TIMEOUT 120
        ENVIRONMENT "ATOM_TEST_SCRIPT=${CMAKE_CURRENT_SOURCE_DIR}/Scenarios/Room/hello_room.atomtest" "ATOM_START_LEVEL=hello_room"
        COMMAND $<TARGET_FILE:HelloAtom>)
endif()
