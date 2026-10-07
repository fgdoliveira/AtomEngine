#pragma once

// Developer switches (M82): every ATOM_* environment variable goes through
// here. In a development build it's the environment's value; in the
// packaged game (-DATOM_DISTRIBUTION=ON, so ATOM_DEV_TOOLS is 0) it's
// always null - a player's game can't be steered by environment
// variables, and this is the one place to audit that.

#include <SDL3/SDL_stdinc.h>

#ifndef ATOM_DEV_TOOLS
#define ATOM_DEV_TOOLS 1
#endif

namespace Atom
{
    inline constexpr bool DevToolsEnabled = ATOM_DEV_TOOLS != 0;

    // The switch's value, or null when unset - or in a distribution build.
    inline const char* DevSwitch(const char* name)
    {
#if ATOM_DEV_TOOLS
        return SDL_getenv(name);
#else
        (void)name;
        return nullptr;
#endif
    }
}
