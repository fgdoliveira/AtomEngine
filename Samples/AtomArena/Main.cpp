// ATOM ARENA (v0.0.15, M93): a survival arena built to measure - see
// ArenaApp.h and docs/Architecture.md (ADR-010).
#include "ArenaApp.h"

#include <SDL3/SDL_main.h> // the Windows entry point of a windowed program

int main(int /*argc*/, char** /*argv*/)
{
    Arena::ArenaApp app;
    return app.Run();
}
