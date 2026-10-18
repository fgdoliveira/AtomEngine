# v0.0.19 (M121): a project fresh from AtomProjectTool configures at once,
# and makes no executable until it has Source/Main.cpp - checked by
# creating one in a scratch workspace and configuring the engine against it
# with the bundled apps off (so it also proves the engine, and since M126
# the workspace, configure with none of them).
#
#   cmake -DENGINE=<engine source> -DTOOL=<AtomProjectTool> -DWORK=<scratch dir> -P EmptyProjectConfigures.cmake
foreach(var ENGINE TOOL WORK)
    if(NOT DEFINED ${var})
        message(FATAL_ERROR "${var} is required")
    endif()
endforeach()

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}/a workspace")
set(workspace "${WORK}/a workspace")

execute_process(COMMAND "${TOOL}" create --workspace "${workspace}" --name EmptyGame
                RESULT_VARIABLE created OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(NOT created EQUAL 0)
    message(FATAL_ERROR "AtomProjectTool failed (${created}): ${out}${err}")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" -S "${ENGINE}" -B "${WORK}/build"
            -DATOM_EXAMPLE_APPS=OFF -DATOM_BUILD_GAME=OFF -DATOM_BUILD_TESTS=OFF
            "-DATOM_PROJECTS_DIR=${workspace}/Apps"
    RESULT_VARIABLE configured OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(NOT configured EQUAL 0)
    message(FATAL_ERROR "configure failed (${configured}):\n${out}\n${err}")
endif()
if(NOT out MATCHES "Project: EmptyGame")
    message(FATAL_ERROR "the project wasn't discovered:\n${out}")
endif()
if(NOT out MATCHES "EmptyGame: no Source/Main.cpp yet - nothing to build")
    message(FATAL_ERROR "the empty project should have said it builds nothing:\n${out}")
endif()
file(GLOB_RECURSE generated "${WORK}/build/*EmptyGame.vcxproj")
if(generated)
    message(FATAL_ERROR "an executable target was generated for an empty project: ${generated}")
endif()
# M126: the workspace stands without the bundled apps - its target is
# there, theirs aren't.
if(NOT EXISTS "${WORK}/build/Tools/Workspace/AtomWorkspace.vcxproj" AND NOT EXISTS "${WORK}/build/Tools/Workspace/Makefile"
   AND NOT EXISTS "${WORK}/build/Tools/Workspace/build.ninja")
    file(GLOB_RECURSE workspace "${WORK}/build/*AtomWorkspace.vcxproj")
    if(NOT workspace)
        message(FATAL_ERROR "no AtomWorkspace target with the bundled apps off")
    endif()
endif()
file(GLOB_RECURSE examples "${WORK}/build/*Demo.vcxproj" "${WORK}/build/*HelloAtom.vcxproj")
if(examples)
    message(FATAL_ERROR "ATOM_EXAMPLE_APPS=OFF still generated the bundled apps: ${examples}")
endif()
message(STATUS "PASS: EmptyGame configured, discovered, no executable; the workspace without the bundled apps")
file(REMOVE_RECURSE "${WORK}")
