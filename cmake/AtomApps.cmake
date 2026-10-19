# v0.0.16 (M108): each app runs from a folder of its own,
# bin/<config>/<app>/, holding everything it needs - the executable,
# SDL3.dll, the engine's shaders and its assets - and nothing another app
# put there. Before, every app shared bin/<config>/: a file one app copied
# could hide a folder another app forgot to declare.
#
#   atom_app_payload(<app target> [COPY <source folder> <folder in the payload>]...)
#
# COPY: the app's asset folders, declared by the app, copied (if changed)
# after each build into <payload>/<folder in the payload>. What players
# see inside the folder (Assets/<App>/, Assets/<folder>) is unchanged.
# The shaders build once (Shaders/, ATOM_SHADER_OUTPUT_DIR) and are copied
# into each payload; nothing is compiled per app.
#
# v0.0.20 (M127): also for a game outside the engine (its own CMake adds
# the engine below it). Such a target isn't in the engine's folder, so it
# doesn't inherit the engine's C++ runtime setting: it is set here, the
# same static runtime every engine library links (a mismatch is LNK2038).
function(atom_app_payload target)
    cmake_parse_arguments(PARSE_ARGV 1 ARG "" "" "COPY")
    list(LENGTH ARG_COPY count)
    math(EXPR odd "${count} % 2")
    if(odd)
        message(FATAL_ERROR "atom_app_payload(${target}): COPY takes pairs - <source folder> <folder in the payload>")
    endif()

    set_target_properties(${target} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>/${target}"
        MSVC_RUNTIME_LIBRARY "${ATOM_MSVC_RUNTIME}")
    set(payload "$<TARGET_FILE_DIR:${target}>")
    set(commands COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:SDL3::SDL3> "${payload}")
    if(TARGET AtomShaders)
        add_dependencies(${target} AtomShaders)
        list(APPEND commands COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different "${ATOM_SHADER_OUTPUT_DIR}" "${payload}/shaders")
    endif()
    while(ARG_COPY)
        list(POP_FRONT ARG_COPY source destination)
        list(APPEND commands COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different "${source}" "${payload}/${destination}")
    endwhile()
    add_custom_command(TARGET ${target} POST_BUILD ${commands} COMMENT "Staging ${target}'s own folder (its payload)" VERBATIM)
endfunction()
