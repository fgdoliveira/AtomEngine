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

        // The project's build integration: found by the engine's CMake
        // (Apps/*/*.atomproject.json, M121), it adds an executable only when
        // there is code to build.
        const char* CMakeTemplate = R"(# @NAME@ - an AtomEngine project (made by AtomProjectTool / Atom Workspace).
# The engine's CMake finds this folder through @NAME@.atomproject.json.
#
# There is no executable until Source/Main.cpp exists: add it (see README.md),
# then configure again. Everything under Source/ is compiled.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Source/Main.cpp")
    file(GLOB_RECURSE @NAME@_SOURCES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/Source/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/Source/*.h")
    add_executable(@NAME@ ${@NAME@_SOURCES})
    target_link_libraries(@NAME@ PRIVATE AtomEngine AtomFramework)
    # Its own folder, bin/<config>/@NAME@/: SDL3.dll, the engine's shaders
    # and its assets in Assets/@NAME@/ (cmake/AtomApps.cmake).
    atom_app_payload(@NAME@ COPY "${CMAKE_CURRENT_SOURCE_DIR}/Assets" "Assets/@NAME@")
else()
    message(STATUS "@NAME@: no Source/Main.cpp yet - nothing to build")
endif()
)";

        const char* ReadmeTemplate = R"(# @NAME@

An AtomEngine project. `@NAME@.atomproject.json` says what it is; the
engine's build and Atom Workspace find it from there.

| Folder | Holds |
|---|---|
| `Source/` | the C++ code. The executable is built once `Source/Main.cpp` exists |
| `Authoring/` | editable art sources (Blender files, images to export) - never loaded by the game |
| `Assets/` | what the game loads: `Levels/`, `Models/`, `Textures/`, `Audio/` |
| `Config/` | settings files the project ships |
| `Tests/` | CPU tests |
| `Scenarios/` | scripted runs (`*.atomtest`) |

## Start

1. Add `Source/Main.cpp` - an `Atom::Application` with a `main` (see the
   engine's `Apps/Samples/HelloAtom/Main.cpp` and `docs/Getting-Started.md`).
2. Configure and build again; `@NAME@` appears as a target.
3. Put a level in `Assets/Levels/` and check it: `AtomLevelCheck --project @NAME@.atomproject.json`.
)";

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
    }

    const std::vector<std::string>& ProjectFolders()
    {
        static const std::vector<std::string> Folders = {
            "Source", "Authoring", "Assets/Levels", "Assets/Models", "Assets/Textures", "Assets/Audio",
            "Config", "Tests", "Scenarios" };
        return Folders;
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
        const fs::path workspace = fs::path(request.workspace);
        if (request.workspace.empty() || !fs::is_directory(workspace, error))
        {
            result.error = "Workspace: \"" + request.workspace + "\" is not a folder";
            return result;
        }
        const fs::path apps = workspace / "Apps";
        const fs::path destination = apps / request.name;
        if (!Inside(workspace, destination))
        {
            result.error = "Destination leaves the workspace";
            return result;
        }
        if (fs::exists(destination, error))
        {
            result.error = "\"" + destination.string() + "\" already exists - a project is never overwritten";
            return result;
        }
        // Another name that differs only in case is the same folder on
        // Windows: refuse it rather than write into it.
        if (fs::is_directory(apps, error))
        {
            for (const auto& entry : fs::directory_iterator(apps, error))
            {
                std::string existing = entry.path().filename().string();
                std::string wanted = request.name;
                for (auto* s : { &existing, &wanted })
                {
                    for (char& c : *s)
                    {
                        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                    }
                }
                if (existing == wanted)
                {
                    result.error = "\"" + entry.path().filename().string() + "\" already exists in Apps/ (names ignore case)";
                    return result;
                }
            }
        }

        // Everything in a temporary sibling first.
        const fs::path staging = apps / ("." + request.name + ".creating");
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
            || !WriteText(staging / "README.md", Replace(ReadmeTemplate, "@NAME@", request.name)))
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
