// A game outside the engine (v0.0.20, M127-M128) - see CMakeLists.txt. Its
// checks are compiling and linking at all, then finding its staged assets
// as an app does; it then says so and exits.
#include "Core/Application.h" // C++20 headers: compiles only if the standard arrived
#include "Core/DevSwitch.h"
#include "Level/LevelData.h"
#include "Platform/AssetRoots.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <cstdio>
#include <filesystem>
#include <string>

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

    // atom_add_project staged the project's assets as Assets/<Name>/
    // (ATOM_PROJECT_ASSETS) and the shared Kit as Assets/Kit/: found the way
    // an app's AssetRoots finds them.
    const char* base = SDL_GetBasePath();
    const std::string root = base ? base : "";
    const AtomFramework::AssetRoots assets({ root + ATOM_PROJECT_ASSETS, root + "Assets/" });
    std::error_code error;
    const bool own = std::filesystem::is_regular_file(assets.Resolve("Data/consumer.txt"), error);
    const bool shared = std::filesystem::is_directory(assets.Resolve("Kit"), error);

    std::printf("SDK consumer: linked AtomEngine and AtomFramework (dev tools %d, framework says: %s); "
                "assets: own %s, shared Kit %s\n",
                ATOM_DEV_TOOLS, missing.level ? "a level?" : "no level, as expected", own ? "found" : "MISSING",
                shared ? "found" : "MISSING");
    return (missing.level || !own || !shared) ? 1 : 0;
}
