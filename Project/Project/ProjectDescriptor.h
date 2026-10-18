#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace AtomProject
{
    // What a project is (v0.0.19): <Name>.atomproject.json at the project's
    // root. It says what the project is and where its parts are, relative
    // to that file - never where the engine or a build lives on this
    // machine, so the folder can move, be zipped or be cloned.
    struct ProjectDescriptor
    {
        static constexpr int SchemaVersion = 1;

        std::string name;          // also the folder's name
        std::string engine;        // the engine version it was made with ("0.0.19")
        std::string source = "Source";
        std::string assets = "Assets";
        std::string executable;    // the CMake target, once Source/Main.cpp exists
        std::string startupLevel;  // optional: a level in <assets>/Levels/
    };

    struct DescriptorLoad
    {
        std::optional<ProjectDescriptor> descriptor;
        std::string error; // "" when read
    };

    DescriptorLoad ParseDescriptor(std::string_view json);
    std::string WriteDescriptor(const ProjectDescriptor& descriptor);
    DescriptorLoad LoadDescriptor(const std::string& path);

    // "" when `name` can be a project (and a C++ identifier, a CMake target,
    // a folder on every OS); otherwise why not. A letter first, then
    // letters, digits or '_'; at most 64; never a Windows device name.
    std::string CheckProjectName(std::string_view name);

    // "" when `path` is relative and stays inside the project (no root, no
    // drive, no "..").
    std::string CheckRelativePath(std::string_view path);
}
