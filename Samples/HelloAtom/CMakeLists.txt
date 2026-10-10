# HelloAtom (v0.0.14, M91): the smallest app on the engine and the framework
# - see docs/Getting-Started.md. Built in every configuration, CI included
# (it needs no shader compiler to compile); its shaders come with the game
# build, which running it needs.
add_executable(HelloAtom Main.cpp)
target_link_libraries(HelloAtom PRIVATE AtomEngine AtomFramework)
if(TARGET AtomShaders)
    add_dependencies(HelloAtom AtomShaders) # the engine's shaders, shared with every app
endif()

# Its assets beside the executable: its own in Assets/HelloAtom/ (looked up
# first), the shared pieces it uses from Content/ in Assets/<folder>.
set(HELLO_COPY_COMMANDS
    COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/Assets $<TARGET_FILE_DIR:HelloAtom>/Assets/HelloAtom)
foreach(folder IN ITEMS First Kit)
    list(APPEND HELLO_COPY_COMMANDS
        COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
            ${CMAKE_SOURCE_DIR}/Content/${folder} $<TARGET_FILE_DIR:HelloAtom>/Assets/${folder})
endforeach()
add_custom_command(TARGET HelloAtom POST_BUILD ${HELLO_COPY_COMMANDS} COMMENT "Copying HelloAtom's assets" VERBATIM)
