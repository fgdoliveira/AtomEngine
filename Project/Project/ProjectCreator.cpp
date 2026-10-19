#include "Project/ProjectCreator.h"

#include <cctype>
#include <filesystem>
#include <fstream>
#include <system_error>

namespace AtomProject
{
    namespace fs = std::filesystem;

    namespace
    {
        std::string Replace(std::string text, const std::string& token, const std::string& value)
        {
            for (std::size_t at = text.find(token); at != std::string::npos; at = text.find(token, at + value.size()))
            {
                text.replace(at, token.size(), value);
            }
            return text;
        }

        // The project's own build (v0.0.20): it is the top-level project and
        // adds the engine below it; atom_add_project (the engine's
        // cmake/AtomProject.cmake) makes the executable from the descriptor.
        // Raw strings with a delimiter of their own: CMake and JSON text holds
        // ')"', which would end a plain R"(...)" early.
        const char* CMakeTemplate = R"tmpl(# @NAME@ - an AtomEngine project (made by AtomProjectTool / Atom Workspace).
# It owns its build: the engine is added below it, and atom_add_project makes
# the executable that @NAME@.atomproject.json names, from Source/.
#
#   cmake --preset atom          (CMakeUserPresets.json: this machine's engine)
#   cmake --preset default       (the ATOM_ENGINE_DIR environment variable)
#   cmake --build build --config Debug
#
# There is no executable until Source/Main.cpp exists (see README.md).
cmake_minimum_required(VERSION 3.26)
project(@NAME@ LANGUAGES CXX)

set(ATOM_ENGINE_DIR "$ENV{ATOM_ENGINE_DIR}" CACHE PATH "The AtomEngine folder this project builds against")
if(NOT EXISTS "${ATOM_ENGINE_DIR}/cmake/AtomProject.cmake")
    message(FATAL_ERROR "@NAME@ needs the AtomEngine folder: configure with --preset atom (CMakeUserPresets.json), "
                        "or set ATOM_ENGINE_DIR (got '${ATOM_ENGINE_DIR}').")
endif()
add_subdirectory("${ATOM_ENGINE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/atom")

# Shared pieces of the engine's Content/ this project uses: CONTENT Kit Sky ...
atom_add_project()
)tmpl";

        const char* PresetsTemplate = R"tmpl({
  "version": 6,
  "configurePresets": [
    {
      "name": "atom-base",
      "hidden": true,
      "binaryDir": "${sourceDir}/build"
    },
    {
      "name": "default",
      "displayName": "@NAME@ (engine from ATOM_ENGINE_DIR)",
      "inherits": "atom-base",
      "cacheVariables": { "ATOM_ENGINE_DIR": "$env{ATOM_ENGINE_DIR}" }
    }
  ]
}
)tmpl";

        const char* UserPresetsTemplate = R"tmpl({
  "version": 6,
  "configurePresets": [
    {
      "name": "atom",
      "displayName": "This machine's AtomEngine",
      "inherits": "atom-base",
      "cacheVariables": { "ATOM_ENGINE_DIR": "@ENGINE@" }
    }
  ]
}
)tmpl";

        const char* GitignoreTemplate = R"tmpl(# The build, and this machine's engine path (CMakeUserPresets.json).
build/
CMakeUserPresets.json
)tmpl";

        const char* ReadmeTemplate = R"tmpl(# @NAME@

An AtomEngine project. `@NAME@.atomproject.json` says what it is;
`CMakeLists.txt` builds it, with the engine added below it.

| Folder | Holds |
|---|---|
| `Source/` | the C++ code. The executable is built once `Source/Main.cpp` exists |
| `Authoring/` | editable art sources (Blender files, images to export) - never loaded by the game |
| `Assets/` | what the game loads: `Levels/`, `Models/`, `Textures/`, `Audio/` |
| `Config/` | settings files the project ships |
| `Tests/` | CPU tests |
| `Scenarios/` | scripted runs (`*.atomtest`) |

## Build

`CMakeUserPresets.json` (not committed) names this machine's engine:

    cmake --preset atom
    cmake --build build --config Debug

Elsewhere, set `ATOM_ENGINE_DIR` and use `--preset default`, or pass
`-DATOM_ENGINE_DIR=<engine>`. Atom Workspace does all of this for you.

## Start

1. Add `Source/Main.cpp` - an `Atom::Application` with a `main` (see the
   engine's `Apps/Samples/HelloAtom/Main.cpp` and `docs/Getting-Started.md`).
   Its assets are found at `Assets/@NAME@/` beside the executable
   (`ATOM_PROJECT_ASSETS`), shared ones at `Assets/`.
2. Name the engine's shared content it uses in `CMakeLists.txt`:
   `atom_add_project(CONTENT Kit)`.
3. Put a level in `Assets/Levels/` and check it: `AtomLevelCheck --project @NAME@.atomproject.json`.
)tmpl";

        bool WriteText(const fs::path& path, const std::string& text)
        {
            std::ofstream file(path, std::ios::binary | std::ios::trunc);
            file << text;
            return static_cast<bool>(file);
        }

        // `child` lies inside `parent` once both are made absolute and
        // normalised (".." and "." resolved, without touching the disk).
        bool Inside(const fs::path& parent, const fs::path& child)
        {
            const fs::path p = fs::absolute(parent).lexically_normal();
            const fs::path c = fs::absolute(child).lexically_normal();
            const fs::path relative = c.lexically_relative(p);
            return !relative.empty() && *relative.begin() != ".." && *relative.begin() != ".";
        }

        std::string Lower(std::string text)
        {
            for (char& c : text)
            {
                c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            }
            return text;
        }
    }

    const std::vector<std::string>& ProjectFolders()
    {
        static const std::vector<std::string> Folders = {
            "Source", "Authoring", "Assets/Levels", "Assets/Models", "Assets/Textures", "Assets/Audio",
            "Config", "Tests", "Scenarios" };
        return Folders;
    }

    std::string UserPresets(const std::string& engineDir)
    {
        // A JSON string: forward slashes need no escaping; a quote or a
        // backslash left in a path would.
        std::string path = fs::path(engineDir).generic_string();
        std::string escaped;
        for (const char c : path)
        {
            if (c == '"' || c == '\\')
            {
                escaped += '\\';
            }
            escaped += c;
        }
        return Replace(UserPresetsTemplate, "@ENGINE@", escaped);
    }

    CreateResult CreateProject(const CreateRequest& request)
    {
        CreateResult result;
        if (const std::string problem = CheckProjectName(request.name); !problem.empty())
        {
            result.error = "Name: " + problem;
            return result;
        }
        std::error_code error;
        const fs::path location = fs::path(request.location);
        if (request.location.empty() || !fs::is_directory(location, error))
        {
            result.error = "Location: \"" + request.location + "\" is not a folder";
            return result;
        }
        const fs::path destination = location / request.name;
        if (!Inside(location, destination))
        {
            result.error = "Destination leaves the location";
            return result;
        }
        if (fs::exists(destination, error))
        {
            result.error = "\"" + destination.string() + "\" already exists - a project is never overwritten";
            return result;
        }
        // Another name that differs only in case is the same folder on
        // Windows: refuse it rather than write into it.
        for (const auto& entry : fs::directory_iterator(location, error))
        {
            if (Lower(entry.path().filename().string()) == Lower(request.name))
            {
                result.error = "\"" + entry.path().filename().string() + "\" already exists there (names ignore case)";
                return result;
            }
        }

        // Everything in a temporary sibling first.
        const fs::path staging = location / ("." + request.name + ".creating");
        fs::remove_all(staging, error); // a leftover from a crash is ours to clear
        error.clear();
        const auto fail = [&](const std::string& why) {
            std::error_code ignored;
            fs::remove_all(staging, ignored);
            result.error = why;
            return result;
        };
        for (const std::string& folder : ProjectFolders())
        {
            if (!fs::create_directories(staging / folder, error) && error)
            {
                return fail("Cannot create " + (staging / folder).string() + ": " + error.message());
            }
            // Empty folders don't survive git: each keeps a placeholder.
            if (!WriteText(staging / folder / ".gitkeep", ""))
            {
                return fail("Cannot write in " + (staging / folder).string());
            }
        }
        ProjectDescriptor descriptor;
        descriptor.name = request.name;
        descriptor.engine = request.engineVersion;
        descriptor.executable = request.name;
        const std::string descriptorName = request.name + ".atomproject.json";
        if (!WriteText(staging / descriptorName, WriteDescriptor(descriptor))
            || !WriteText(staging / "CMakeLists.txt", Replace(CMakeTemplate, "@NAME@", request.name))
            || !WriteText(staging / "CMakePresets.json", Replace(PresetsTemplate, "@NAME@", request.name))
            || !WriteText(staging / ".gitignore", GitignoreTemplate)
            || !WriteText(staging / "README.md", Replace(ReadmeTemplate, "@NAME@", request.name))
            || (!request.engineDir.empty() && !WriteText(staging / "CMakeUserPresets.json", UserPresets(request.engineDir))))
        {
            return fail("Cannot write the project's files in " + staging.string());
        }
        // Read back what was written: the descriptor must parse.
        if (const DescriptorLoad check = LoadDescriptor((staging / descriptorName).string()); !check.descriptor)
        {
            return fail("The new descriptor doesn't read back: " + check.error);
        }

        fs::rename(staging, destination, error);
        if (error)
        {
            return fail("Cannot move the project into place (" + destination.string() + "): " + error.message());
        }
        result.ok = true;
        result.projectDir = destination.string();
        result.descriptorPath = (destination / descriptorName).string();
        return result;
    }
}
