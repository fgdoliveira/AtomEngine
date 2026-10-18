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
    struct CreateRequest
    {
        std::string workspace;      // the folder holding Apps/ (the engine's root, or another)
        std::string name;
        std::string engineVersion;  // written into the descriptor
    };

    struct CreateResult
    {
        bool ok = false;
        std::string projectDir;     // Apps/<Name>/ - only when ok
        std::string descriptorPath;
        std::string error;          // why not, for a person to act on
    };

    // Never overwrites: an existing destination is an error. Built in a
    // temporary sibling (Apps/.<Name>.creating) and renamed into place only
    // when everything was written, so a failure leaves nothing behind.
    CreateResult CreateProject(const CreateRequest& request);

    // The folders a new project has, relative to its root.
    const std::vector<std::string>& ProjectFolders();
}
