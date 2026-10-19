// A game outside the engine (v0.0.20, M127) - see CMakeLists.txt. Its
// checks are compiling and linking at all; it then says so and exits.
#include "Core/Application.h" // C++20 headers: compiles only if the standard arrived
#include "Core/DevSwitch.h"
#include "Level/LevelData.h"

#include <SDL3/SDL_main.h>

#include <cstdio>

// The engine's value of ATOM_DEV_TOOLS must reach a game's code: without it
// Core/DevSwitch.h would quietly pick its own default (an ODR violation
// against a distribution-built engine).
#ifndef ATOM_DEV_TOOLS
#error "ATOM_DEV_TOOLS didn't arrive with AtomEngine's usage requirements"
#endif

namespace
{
    // Deriving from Application pulls in the engine's objects - the window,
    // the renderer, audio - so a C++ runtime mismatch fails the link here.
    class Consumer final : public Atom::Application
    {
    };
}

int main(int /*argc*/, char** /*argv*/)
{
    Consumer consumer; // constructed, never run: no window in a test
    const AtomFramework::LevelParseResult missing = AtomFramework::LoadLevelFile("no such level.json");
    std::printf("SDK consumer: linked AtomEngine and AtomFramework (dev tools %d, framework says: %s)\n",
                ATOM_DEV_TOOLS, missing.level ? "a level?" : "no level, as expected");
    return missing.level ? 1 : 0;
}
