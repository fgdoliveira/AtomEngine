# v0.0.20 (M128): a project's build from its descriptor - a thin wrapper
# over ordinary CMake targets, not a build system of its own. The
# descriptor (<Name>.atomproject.json, v0.0.19) says what the project is
# and where its parts are; CMake still decides how it builds. Nothing here
# reads source lists, flags or dependencies from JSON.
#
#   atom_add_project([MANIFEST <file>] [CONTENT <folder>...])
#
# MANIFEST  the descriptor; default: the one *.atomproject.json in the
#           calling folder.
# CONTENT   folders of the engine's shared Content/ the project uses (Kit,
#           Sky, Fonts...), staged as Assets/<folder> beside its own.
#
# It makes the executable named by the descriptor from every .cpp/.h under
# its source folder - only once <source>/Main.cpp exists (a fresh project
# configures and builds nothing) - links AtomEngine and AtomFramework, and
# stages its folder with atom_app_payload: SDL3.dll, the shaders, its
# assets as Assets/<Name>/ and the CONTENT folders. The code finds them as
# an app does: AssetRoots({ base + ATOM_PROJECT_ASSETS, base + "Assets/" }).
# Sets ATOM_PROJECT_TARGET in the caller ("" when there's nothing to build).

# Reads and checks a descriptor; sets <prefix>_NAME, _EXECUTABLE, _SOURCE,
# _ASSETS, _STARTUP_LEVEL and _DIR in the caller. Script-safe (cmake -P):
# Tests/SDK/ReadProject.cmake checks its rules.
function(atom_read_project manifest prefix)
    if(NOT EXISTS "${manifest}")
        message(FATAL_ERROR "atom_read_project: no descriptor at '${manifest}'")
    endif()
    file(READ "${manifest}" json)
    string(JSON schema ERROR_VARIABLE error GET "${json}" schema)
    if(error OR NOT schema EQUAL 1)
        message(FATAL_ERROR "${manifest}: \"schema\" must be 1 (this engine reads schema 1)")
    endif()
    string(JSON name ERROR_VARIABLE error GET "${json}" name)
    if(error OR NOT name MATCHES "^[A-Za-z][A-Za-z0-9_]*$")
        message(FATAL_ERROR "${manifest}: \"name\" must be a letter, then letters, digits or '_' (got '${name}')")
    endif()
    # The optional fields keep the C++ parser's defaults (ProjectDescriptor.cpp).
    set(defaults executable "${name}" source "Source" assets "Assets" startupLevel "")
    while(defaults)
        list(POP_FRONT defaults key fallback)
        string(JSON value ERROR_VARIABLE error GET "${json}" ${key})
        if(error OR value STREQUAL "")
            set(value "${fallback}")
        endif()
        set(${key} "${value}")
    endwhile()
    foreach(key source assets)
        # Relative, inside the project - never a machine's path.
        if(IS_ABSOLUTE "${${key}}" OR "${${key}}" MATCHES "(^|[/\\\\])\\.\\.([/\\\\]|$)" OR "${${key}}" MATCHES "^[A-Za-z]:")
            message(FATAL_ERROR "${manifest}: \"${key}\" must be a folder inside the project (got '${${key}}')")
        endif()
    endforeach()
    if(NOT executable MATCHES "^[A-Za-z][A-Za-z0-9_]*$")
        message(FATAL_ERROR "${manifest}: \"executable\" must be a CMake target name (got '${executable}')")
    endif()
    get_filename_component(dir "${manifest}" DIRECTORY)
    set(${prefix}_NAME "${name}" PARENT_SCOPE)
    set(${prefix}_EXECUTABLE "${executable}" PARENT_SCOPE)
    set(${prefix}_SOURCE "${source}" PARENT_SCOPE)
    set(${prefix}_ASSETS "${assets}" PARENT_SCOPE)
    set(${prefix}_STARTUP_LEVEL "${startupLevel}" PARENT_SCOPE)
    set(${prefix}_DIR "${dir}" PARENT_SCOPE)
endfunction()

function(atom_add_project)
    cmake_parse_arguments(PARSE_ARGV 0 ARG "" "MANIFEST" "CONTENT")
    set(ATOM_PROJECT_TARGET "" PARENT_SCOPE)
    if(NOT ARG_MANIFEST)
        file(GLOB found "${CMAKE_CURRENT_SOURCE_DIR}/*.atomproject.json")
        list(LENGTH found count)
        if(NOT count EQUAL 1)
            message(FATAL_ERROR "atom_add_project: expected one *.atomproject.json in ${CMAKE_CURRENT_SOURCE_DIR} (found ${count}); name it with MANIFEST")
        endif()
        set(ARG_MANIFEST "${found}")
    endif()
    atom_read_project("${ARG_MANIFEST}" project)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${ARG_MANIFEST}")

    set(source "${project_DIR}/${project_SOURCE}")
    if(NOT EXISTS "${source}/Main.cpp")
        message(STATUS "${project_NAME}: no ${project_SOURCE}/Main.cpp yet - nothing to build")
        return()
    endif()
    # Everything under the source folder; "." (the bundled examples' layout)
    # leaves out their tests, scenarios and any build folder.
    # Matched on the path inside the source folder only: a project that
    # itself lives under a "build" or "Tests" folder keeps its sources.
    file(GLOB_RECURSE found RELATIVE "${source}" CONFIGURE_DEPENDS "${source}/*.cpp" "${source}/*.h")
    list(FILTER found EXCLUDE REGEX "^(build|Tests|Scenarios)/")
    list(TRANSFORM found PREPEND "${source}/" OUTPUT_VARIABLE sources)
    if(NOT "${source}/Main.cpp" IN_LIST sources)
        message(FATAL_ERROR "atom_add_project(${project_NAME}): ${project_SOURCE}/Main.cpp exists but wasn't collected")
    endif()

    set(target "${project_EXECUTABLE}")
    add_executable(${target} ${sources})
    target_link_libraries(${target} PRIVATE AtomEngine AtomFramework)
    target_compile_definitions(${target} PRIVATE ATOM_PROJECT_ASSETS="Assets/${project_NAME}/")

    set(copies "${project_DIR}/${project_ASSETS}" "Assets/${project_NAME}")
    foreach(folder IN LISTS ARG_CONTENT)
        if(NOT IS_DIRECTORY "${ATOM_CONTENT_DIR}/${folder}")
            message(FATAL_ERROR "atom_add_project(${project_NAME}): no shared content folder '${folder}' in ${ATOM_CONTENT_DIR}")
        endif()
        list(APPEND copies "${ATOM_CONTENT_DIR}/${folder}" "Assets/${folder}")
    endforeach()
    atom_app_payload(${target} COPY ${copies})
    set(ATOM_PROJECT_TARGET "${target}" PARENT_SCOPE)
endfunction()
