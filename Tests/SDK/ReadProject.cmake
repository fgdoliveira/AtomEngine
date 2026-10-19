# v0.0.20 (M128): atom_read_project's rules (cmake/AtomProject.cmake), in
# script mode - seconds, no configure. Each case is a descriptor run in a
# process of its own (a rejected one stops its process with FATAL_ERROR).
#
#   cmake -DENGINE=<engine source> -DWORK=<scratch dir> -P ReadProject.cmake
foreach(var ENGINE WORK)
    if(NOT DEFINED ${var})
        message(FATAL_ERROR "${var} is required")
    endif()
endforeach()
include("${ENGINE}/cmake/AtomProject.cmake")

if(DEFINED CASE_FILE) # one case: read it and print what was read
    atom_read_project("${CASE_FILE}" p)
    message(STATUS "read: ${p_NAME}|${p_EXECUTABLE}|${p_SOURCE}|${p_ASSETS}|${p_STARTUP_LEVEL}")
    return()
endif()

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}/a project")
set(failures 0)

# expect(<name> <json> ACCEPT <read line> | REJECT <message part>)
function(expect name json mode expected)
    set(file "${WORK}/a project/${name}.atomproject.json")
    file(WRITE "${file}" "${json}")
    execute_process(COMMAND "${CMAKE_COMMAND}" -DENGINE=${ENGINE} -DWORK=${WORK} "-DCASE_FILE=${file}" -P "${CMAKE_CURRENT_LIST_FILE}"
                    RESULT_VARIABLE result OUTPUT_VARIABLE out ERROR_VARIABLE err)
    set(text "${out}${err}")
    if(mode STREQUAL "ACCEPT" AND result EQUAL 0 AND text MATCHES "read: ${expected}")
        message(STATUS "ok   ${name}")
    elseif(mode STREQUAL "REJECT" AND NOT result EQUAL 0 AND text MATCHES "${expected}")
        message(STATUS "ok   ${name}")
    else()
        message(STATUS "FAIL ${name}: expected ${mode} '${expected}', got (${result}):\n${text}")
        math(EXPR failures "${failures} + 1")
        set(failures ${failures} PARENT_SCOPE)
    endif()
endfunction()

expect(full [=[{ "schema": 1, "name": "Sandbox", "source": "Code", "assets": "Data", "executable": "SandboxGame", "startupLevel": "intro" }]=]
       ACCEPT "Sandbox\\|SandboxGame\\|Code\\|Data\\|intro")
expect(defaults [=[{ "schema": 1, "name": "Sandbox" }]=]
       ACCEPT "Sandbox\\|Sandbox\\|Source\\|Assets\\|")
expect(examples_layout [=[{ "schema": 1, "name": "HelloAtom", "source": ".", "assets": "Assets" }]=]
       ACCEPT "HelloAtom\\|HelloAtom\\|\\.\\|Assets\\|")
expect(newer_schema [=[{ "schema": 2, "name": "Sandbox" }]=] REJECT "\"schema\" must be 1")
expect(no_schema [=[{ "name": "Sandbox" }]=] REJECT "\"schema\" must be 1")
expect(bad_name [=[{ "schema": 1, "name": "2fast" }]=] REJECT "\"name\" must be a letter")
expect(name_with_space [=[{ "schema": 1, "name": "My Game" }]=] REJECT "\"name\" must be a letter")
expect(absolute_assets [=[{ "schema": 1, "name": "Sandbox", "assets": "C:/Games/Assets" }]=] REJECT "\"assets\" must be a folder inside")
expect(escaping_source [=[{ "schema": 1, "name": "Sandbox", "source": "../Engine" }]=] REJECT "\"source\" must be a folder inside")
expect(bad_executable [=[{ "schema": 1, "name": "Sandbox", "executable": "my-game" }]=] REJECT "\"executable\" must be") # CMake wraps long errors: match the start
expect(not_json [=[{ schema: 1 ]=] REJECT "\"schema\" must be 1")

file(REMOVE_RECURSE "${WORK}")
if(failures GREATER 0)
    message(FATAL_ERROR "${failures} case(s) failed")
endif()
message(STATUS "PASS: atom_read_project")
