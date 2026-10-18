// Atom Workspace (v0.0.19): the engine's development program. See
// WorkspaceApp.h; docs/Getting-Started.md creates a project with it.
//
//   AtomWorkspace [--project <Name>.atomproject.json [--level <name>]]
//   AtomWorkspace --test <folder>     the self-test (WorkspaceSelfTest.cpp)
#include "WorkspaceApp.h"

#include <SDL3/SDL_main.h> // the Windows entry point of a windowed program

#include <string_view>

int main(int argc, char** argv)
{
    AtomWorkspace::WorkspaceApp::Startup startup;
    for (int i = 1; i + 1 < argc; i += 2)
    {
        const std::string_view option = argv[i];
        if (option == "--project")
        {
            startup.project = argv[i + 1];
        }
        else if (option == "--level")
        {
            startup.level = argv[i + 1];
        }
        else if (option == "--test")
        {
            startup.testFolder = argv[i + 1];
        }
    }
    AtomWorkspace::WorkspaceApp app;
    app.SetStartup(std::move(startup));
    return app.Run();
}
