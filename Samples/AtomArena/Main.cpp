// ATOM ARENA (v0.0.15, M93): a survival arena built to measure - see
// ArenaApp.h and docs/Architecture.md (ADR-010).
#include "ArenaApp.h"

#include <SDL3/SDL_main.h> // the Windows entry point of a windowed program

#include <string>
#include <vector>

int main(int argc, char** argv)
{
    // --benchmark --scenario <name> [--seed N] [--frames N] [--warmup N] [--out file]
    std::vector<std::string> arguments(argv + 1, argv + argc);
    Arena::ArenaApp app(std::move(arguments));
    return app.Run();
}
