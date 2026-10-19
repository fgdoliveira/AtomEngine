# v0.0.16 (M105): one CPU test executable per owner - the engine, the
# framework, the shared content, each app - so a change builds and runs
# the tests of what it touched, and a test sees only what its owner ships.
#
#   atom_add_tests(<target> OWNER <owner> [ASSETS <dir>] SOURCES ... [LIBS ...])
#
#   OWNER   engine | framework | content | demo | drift | showcase | arena:
#           the CTest label `check.ps1 -Level changed` selects by.
#   ASSETS  the owner's own asset folder; tests resolve it, then the shared
#           Content/ - never another app's (Tests/Support/TestAssets.h).
#
# Each target is registered twice in CTest, by doctest suite:
#   Unit.<owner>     every case outside the suites below  (labels cpu, unit, <owner>)
#   Content.<owner>  the "content" suite: shipped files checked (cpu, content, <owner>)
# The "benchmark" suite (wall-clock comparisons) is never registered:
# `<target> -ts=benchmark` runs it by hand. `AtomTests` builds them all.
set(ATOM_TEST_SUPPORT_DIR "${ATOM_ENGINE_ROOT}/Tests/Support" CACHE INTERNAL "")

function(atom_add_tests target)
    cmake_parse_arguments(PARSE_ARGV 1 ARG "" "OWNER;ASSETS" "SOURCES;LIBS")
    if(NOT ARG_OWNER)
        message(FATAL_ERROR "atom_add_tests(${target}): OWNER is required")
    endif()

    add_executable(${target} ${ATOM_TEST_SUPPORT_DIR}/TestMain.cpp ${ARG_SOURCES})
    target_link_libraries(${target} PRIVATE ${ARG_LIBS})
    target_include_directories(${target} PRIVATE ${ATOM_ENGINE_ROOT}/external/doctest ${ATOM_TEST_SUPPORT_DIR})
    target_compile_definitions(${target} PRIVATE ATOM_SOURCE_DIR="${ATOM_ENGINE_ROOT}")
    if(ARG_ASSETS)
        target_compile_definitions(${target} PRIVATE ATOM_TEST_ASSETS="${ARG_ASSETS}/")
    endif()
    set_property(GLOBAL APPEND PROPERTY ATOM_TEST_TARGETS ${target})

    # Which runs the target has: a source file opening the "content" suite
    # holds content checks; any other test file, unit cases.
    set(has_unit OFF)
    set(has_content OFF)
    foreach(source IN LISTS ARG_SOURCES)
        if(NOT source MATCHES "\\.cpp$")
            continue()
        endif()
        file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/${source}" suite REGEX "TEST_SUITE_BEGIN\\(\"content\"\\)")
        if(suite)
            set(has_content ON)
        else()
            set(has_unit ON)
        endif()
    endforeach()
    if(has_unit)
        add_test(NAME Unit.${ARG_OWNER} COMMAND ${target} -tse=content,benchmark)
        set_tests_properties(Unit.${ARG_OWNER} PROPERTIES LABELS "cpu;unit;${ARG_OWNER}")
    endif()
    if(has_content)
        add_test(NAME Content.${ARG_OWNER} COMMAND ${target} -ts=content)
        set_tests_properties(Content.${ARG_OWNER} PROPERTIES LABELS "cpu;content;${ARG_OWNER}")
    endif()
endfunction()

# In-game scenarios (M107): the real app on a GPU, in a window. Each app
# registers its own, after its executable - so an app that isn't built
# (ATOM_BUILD_GAME=OFF, CI) registers none, and none depends on another.
#
#   atom_add_scenario(<name> OWNER <owner> COMMAND <exe> [args...]
#                     [ENVIRONMENT VAR=value...] [PASS <regex>] [TIMEOUT <s>] [SMOKE])
#   atom_add_scenarios(<exe target> OWNER <owner> DIR <folder> [SMOKE <name>] [TIMEOUT <s>])
#
# The test is Scenario.<owner>.<name>, labelled scenario and <owner>; SMOKE
# adds the label smoke: one short run per app, proving it starts, draws
# and shuts down clean - what an engine or framework change runs in every
# app (`ctest -L smoke`). Every scenario fails on a GPU wrapper outliving
# the renderer (M53) or a pipeline that couldn't be created.
set(ATOM_SCENARIO_FAILURES "GPU resources still alive at shutdown;Failed to create [a-z ]*pipeline")

function(atom_add_scenario name)
    cmake_parse_arguments(PARSE_ARGV 1 ARG "SMOKE" "OWNER;PASS;TIMEOUT" "COMMAND;ENVIRONMENT")
    if(NOT ARG_OWNER OR NOT ARG_COMMAND)
        message(FATAL_ERROR "atom_add_scenario(${name}): OWNER and COMMAND are required")
    endif()
    set(test Scenario.${ARG_OWNER}.${name})
    set(labels scenario ${ARG_OWNER})
    if(ARG_SMOKE)
        list(APPEND labels smoke)
    endif()
    if(NOT ARG_TIMEOUT)
        set(ARG_TIMEOUT 240)
    endif()
    add_test(NAME ${test} COMMAND ${ARG_COMMAND})
    set_tests_properties(${test} PROPERTIES
        LABELS "${labels}"
        FAIL_REGULAR_EXPRESSION "${ATOM_SCENARIO_FAILURES}"
        TIMEOUT ${ARG_TIMEOUT})
    if(ARG_ENVIRONMENT)
        set_tests_properties(${test} PROPERTIES ENVIRONMENT "${ARG_ENVIRONMENT}")
    endif()
    if(ARG_PASS)
        set_tests_properties(${test} PROPERTIES PASS_REGULAR_EXPRESSION "${ARG_PASS}")
    endif()
endfunction()

# Every <folder>/*.atomtest, run by the app with ATOM_TEST_SCRIPT set.
function(atom_add_scenarios exe)
    cmake_parse_arguments(PARSE_ARGV 1 ARG "" "OWNER;DIR;SMOKE;TIMEOUT" "")
    file(GLOB scripts CONFIGURE_DEPENDS "${ARG_DIR}/*.atomtest")
    foreach(script IN LISTS scripts)
        get_filename_component(name "${script}" NAME_WE)
        set(smoke "")
        if(name STREQUAL ARG_SMOKE)
            set(smoke SMOKE)
        endif()
        set(timeout "")
        if(ARG_TIMEOUT)
            set(timeout TIMEOUT ${ARG_TIMEOUT})
        endif()
        atom_add_scenario(${name} OWNER ${ARG_OWNER} COMMAND $<TARGET_FILE:${exe}>
            ENVIRONMENT "ATOM_TEST_SCRIPT=${script}" ${timeout} ${smoke})
    endforeach()
endfunction()
