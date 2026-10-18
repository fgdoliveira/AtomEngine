// Atom Workspace (v0.0.19): the engine's development program. See
// WorkspaceApp.h; docs/Getting-Started.md creates a project with it.
#include "WorkspaceApp.h"

#include <SDL3/SDL_main.h> // the Windows entry point of a windowed program

int main(int /*argc*/, char** /*argv*/)
{
    AtomWorkspace::WorkspaceApp app;
    return app.Run();
}
