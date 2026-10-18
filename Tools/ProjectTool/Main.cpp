// AtomProjectTool (v0.0.19): projects from the command line - the same
// service Atom Workspace's Create Project uses (Project/ProjectCreator).
//
//   AtomProjectTool create --workspace <folder> --name <Name>
//
// Exit codes: 0 created, 1 refused (the reason is printed), 2 usage.
#include "Project/ProjectCreator.h"

#include <iostream>
#include <string>

namespace
{
    int Usage()
    {
        std::cerr << "usage: AtomProjectTool create --workspace <folder> --name <Name>\n"
                     "  Makes <folder>/Apps/<Name>/ with its descriptor, folders and CMakeLists.txt.\n"
                     "  Name: a letter, then letters, digits or '_'. Never overwrites.\n";
        return 2;
    }
}

int main(int argc, char** argv)
{
    if (argc < 2 || std::string(argv[1]) != "create")
    {
        return Usage();
    }
    AtomProject::CreateRequest request;
    request.engineVersion = ATOM_VERSION;
    for (int i = 2; i < argc; ++i)
    {
        const std::string arg = argv[i];
        if (arg == "--workspace" && i + 1 < argc)
        {
            request.workspace = argv[++i];
        }
        else if (arg == "--name" && i + 1 < argc)
        {
            request.name = argv[++i];
        }
        else
        {
            std::cerr << "unknown argument: " << arg << '\n';
            return Usage();
        }
    }
    if (request.workspace.empty() || request.name.empty())
    {
        return Usage();
    }
    const AtomProject::CreateResult result = AtomProject::CreateProject(request);
    if (!result.ok)
    {
        std::cerr << "refused: " << result.error << '\n';
        return 1;
    }
    std::cout << "created " << result.projectDir << "\n"
              << "  " << result.descriptorPath << "\n"
              << "Configure again to see it; it builds an executable once Source/Main.cpp exists.\n";
    return 0;
}
