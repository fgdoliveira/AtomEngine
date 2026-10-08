#pragma once

#include "Level/ScreenProgram.h"

namespace Demo
{
    // The demo's live-screen programs (v0.0.14; the screens themselves are
    // M27, the self-playing game M32): a screen naming a `machine` plays
    // that pachinko playfield by itself; any other runs the attract loop.
    ScreenFactory MakePachinkoScreens();
}
