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
set(ATOM_TEST_SUPPORT_DIR "${CMAKE_SOURCE_DIR}/Tests/Support")

function(atom_add_tests target)
    cmake_parse_arguments(PARSE_ARGV 1 ARG "" "OWNER;ASSETS" "SOURCES;LIBS")
    if(NOT ARG_OWNER)
        message(FATAL_ERROR "atom_add_tests(${target}): OWNER is required")
    endif()

    add_executable(${target} ${ATOM_TEST_SUPPORT_DIR}/TestMain.cpp ${ARG_SOURCES})
    target_link_libraries(${target} PRIVATE ${ARG_LIBS})
    target_include_directories(${target} PRIVATE ${CMAKE_SOURCE_DIR}/external/doctest ${ATOM_TEST_SUPPORT_DIR})
    target_compile_definitions(${target} PRIVATE ATOM_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
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
