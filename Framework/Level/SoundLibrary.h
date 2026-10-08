#pragma once

#include "Audio/AudioSystem.h"

#include <string_view>

namespace AtomFramework
{
    // The sounds a level file names ("wind", "cicadas"...), from the app
    // that runs the level (v0.0.14). Levels start their beds, emitters and
    // movers from it by name; what the names mean is the app's.
    class SoundLibrary
    {
    public:
        virtual ~SoundLibrary() = default;

        // An unknown name gives an empty handle (the voice stays silent).
        virtual Atom::SoundHandle GetSound(std::string_view name) const = 0;
    };
}
