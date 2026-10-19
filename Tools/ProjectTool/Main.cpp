// AtomProjectTool (v0.0.19): projects from the command line - the same
// service Atom Workspace's Create Project uses (Project/ProjectCreator).
//
//   AtomProjectTool create --in <folder> --name <Name> [--engine <folder> | --no-engine]
//
// v0.0.20 (M129): the project is made in <folder>/<Name>/, anywhere, and
// owns its build. --engine names the engine for its CMakeUserPresets.json
// (default: the engine this tool was built from); --no-engine writes none.
// Exit codes: 0 created, 1 refused (the reason is printed), 2 usage.
#include "Project/ProjectCreator.h"

#include <filesystem>
#include <iostream>
#include <string>

namespace
{
    int Usage()
    {
        std::cerr << "usage: AtomProjectTool create --in <folder> --name <Name> [--engine <folder> | --no-engine]\n"
                     "  Makes <folder>/<Name>/: its descriptor, folders, CMakeLists.txt, CMakePresets.json,\n"
                     "  .gitignore and, naming the engine, CMakeUserPresets.json (not committed).\n"
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
    request.engineDir = ATOM_ENGINE_ROOT_DIR; // the engine this tool was built from
    for (int i = 2; i < argc; ++i)
    {
        const std::string arg = argv[i];
        if (arg == "--in" && i + 1 < argc)
        {
            request.location = argv[++i];
        }
        else if (arg == "--name" && i + 1 < argc)
        {
            request.name = argv[++i];
        }
        else if (arg == "--engine" && i + 1 < argc)
        {
            request.engineDir = argv[++i];
        }
        else if (arg == "--no-engine")
        {
            request.engineDir.clear();
        }
        else
        {
            std::cerr << "unknown argument: " << arg << '\n';
            return Usage();
        }
    }
    if (request.location.empty() || request.name.empty())
    {
        return Usage();
    }
    std::error_code error;
    if (!request.engineDir.empty() && !std::filesystem::exists(std::filesystem::path(request.engineDir) / "cmake" / "AtomProject.cmake", error))
    {
        std::cerr << "refused: --engine \"" << request.engineDir << "\" is not an AtomEngine folder\n";
        return 1;
    }
    const AtomProject::CreateResult result = AtomProject::CreateProject(request);
    if (!result.ok)
    {
        std::cerr << "refused: " << result.error << '\n';
        return 1;
    }
    std::cout << "created " << result.projectDir << "\n"
              << "  " << result.descriptorPath << "\n"
              << (request.engineDir.empty() ? "Build it with -DATOM_ENGINE_DIR=<engine> (or --preset default and ATOM_ENGINE_DIR).\n"
                                            : "Build it: cmake --preset atom, then cmake --build build.\n")
              << "It builds an executable once Source/Main.cpp exists.\n";
    return 0;
}
