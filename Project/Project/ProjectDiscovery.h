#pragma once

#include "Project/ProjectDescriptor.h"

#include <string>
#include <vector>

namespace AtomProject
{
    struct DiscoveredProject
    {
        ProjectDescriptor descriptor;
        std::string descriptorPath;
        std::string directory; // the project's root (the descriptor's folder)
        std::string AssetsDir() const;
    };

    struct Discovery
    {
        std::vector<DiscoveredProject> projects; // sorted by name
        std::vector<std::string> errors;         // descriptors that didn't read, each with its file
    };

    // Every <Name>.atomproject.json under `appsDir`, at most `maxDepth`
    // folders down (1: Apps/<Name>/, new projects; 2: Apps/<Group>/<Name>/,
    // the bundled examples). A broken descriptor is reported, never skipped
    // silently.
    Discovery FindProjects(const std::string& appsDir, int maxDepth = 2);
}
