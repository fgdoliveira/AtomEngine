#pragma once

#include "Project/ProjectDescriptor.h"

#include <string>
#include <vector>

namespace AtomProject
{
    // Creating a project (v0.0.19): the same service behind the workspace's
    // Create Project and `AtomProjectTool create`. It makes folders, the
    // descriptor and the build integration - no gameplay, no scene, no
    // Main.cpp: the project configures at once, and builds an executable
    // only once Source/Main.cpp exists.
    //
    // v0.0.20 (M129): a project owns its build. It is made anywhere - in
    // <location>/<Name>/, its own repository if you like - with a top-level
    // CMakeLists.txt that adds the engine below it (add_subdirectory) and
    // builds itself with atom_add_project, a CMakePresets.json, and a
    // .gitignore. Where the engine is on this machine never goes in a
    // tracked file: CMakeUserPresets.json (git-ignored) holds it, or the
    // ATOM_ENGINE_DIR environment variable.
    struct CreateRequest
    {
        std::string location;       // the folder the project's folder is made in
        std::string name;
        std::string engineVersion;  // written into the descriptor
        std::string engineDir;      // this engine's folder, for CMakeUserPresets.json ("" = write none)
    };

    struct CreateResult
    {
        bool ok = false;
        std::string projectDir;     // <location>/<Name>/ - only when ok
        std::string descriptorPath;
        std::string error;          // why not, for a person to act on
    };

    // Never overwrites: an existing destination is an error. Built in a
    // temporary sibling (<location>/.<Name>.creating) and renamed into place
    // only when everything was written, so a failure leaves nothing behind.
    CreateResult CreateProject(const CreateRequest& request);

    // The folders a new project has, relative to its root.
    const std::vector<std::string>& ProjectFolders();

    // CMakeUserPresets.json for a project: the "atom" preset, naming the
    // engine's folder (written with forward slashes). Rewritten when the
    // engine moves; never committed.
    std::string UserPresets(const std::string& engineDir);
}
