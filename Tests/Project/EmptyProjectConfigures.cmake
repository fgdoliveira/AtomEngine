# A project fresh from AtomProjectTool configures on its own and makes no
# executable until it has Source/Main.cpp (v0.0.19, M121; v0.0.20, M129:
# the project is the top-level CMake, the engine below it, configured with
# its own preset from CMakeUserPresets.json - outside the repository's
# build). Then the engine on its own with the bundled apps off: the
# workspace still configures without them (M126).
#
#   cmake -DENGINE=<engine source> -DTOOL=<AtomProjectTool> -DWORK=<scratch dir> -P EmptyProjectConfigures.cmake
foreach(var ENGINE TOOL WORK)
    if(NOT DEFINED ${var})
        message(FATAL_ERROR "${var} is required")
    endif()
endforeach()

file(REMOVE_RECURSE "${WORK}")
set(location "${WORK}/my games")
file(MAKE_DIRECTORY "${location}")

execute_process(COMMAND "${TOOL}" create --in "${location}" --name EmptyGame --engine "${ENGINE}"
                RESULT_VARIABLE created OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(NOT created EQUAL 0)
    message(FATAL_ERROR "AtomProjectTool failed (${created}): ${out}${err}")
endif()
set(project "${location}/EmptyGame")
foreach(file EmptyGame.atomproject.json CMakeLists.txt CMakePresets.json CMakeUserPresets.json .gitignore)
    if(NOT EXISTS "${project}/${file}")
        message(FATAL_ERROR "the new project has no ${file}")
    endif()
endforeach()

# The project's own configure, through its preset (the engine's path from
# CMakeUserPresets.json): the way a developer, or the workspace, runs it.
execute_process(COMMAND "${CMAKE_COMMAND}" --preset atom -DATOM_BUILD_SHADERS=OFF
                WORKING_DIRECTORY "${project}"
                RESULT_VARIABLE configured OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(NOT configured EQUAL 0)
    message(FATAL_ERROR "the project didn't configure (${configured}):\n${out}\n${err}")
endif()
if(NOT out MATCHES "EmptyGame: no Source/Main.cpp yet - nothing to build")
    message(FATAL_ERROR "the empty project should have said it builds nothing:\n${out}")
endif()
file(GLOB_RECURSE generated "${project}/build/*EmptyGame.vcxproj")
if(generated)
    message(FATAL_ERROR "an executable target was generated for an empty project: ${generated}")
endif()
file(GLOB_RECURSE tools "${project}/build/*AtomWorkspace.vcxproj" "${project}/build/*Demo.vcxproj")
if(tools)
    message(FATAL_ERROR "the engine built tools or examples under a project: ${tools}")
endif()

# The engine on its own, the bundled apps off: the workspace stands without them.
execute_process(
    COMMAND "${CMAKE_COMMAND}" -S "${ENGINE}" -B "${WORK}/engine"
            -DATOM_EXAMPLE_APPS=OFF -DATOM_BUILD_GAME=OFF -DATOM_BUILD_TESTS=OFF
    RESULT_VARIABLE configured OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(NOT configured EQUAL 0)
    message(FATAL_ERROR "the engine didn't configure without its apps (${configured}):\n${out}\n${err}")
endif()
file(GLOB_RECURSE workspace "${WORK}/engine/*AtomWorkspace.vcxproj")
if(NOT workspace)
    message(FATAL_ERROR "no AtomWorkspace target with the bundled apps off")
endif()
file(GLOB_RECURSE examples "${WORK}/engine/*Demo.vcxproj" "${WORK}/engine/*HelloAtom.vcxproj")
if(examples)
    message(FATAL_ERROR "ATOM_EXAMPLE_APPS=OFF still generated the bundled apps: ${examples}")
endif()
message(STATUS "PASS: EmptyGame configured on its own, no executable; the workspace without the bundled apps")
file(REMOVE_RECURSE "${WORK}")
