#pragma once

#include "Audio/AudioSystem.h"

#include <cstdint>

namespace AtomGame
{
    // Procedural sound design for the demo. Every generator is seeded, so
    // the same sounds come out on every run. Loops are crossfaded at their
    // seam and repeat without clicks.
    namespace SoundSynth
    {
        enum class Surface
        {
            Asphalt,
            Concrete,
            Dirt,
            Stone,
            Wood, // floorboards: hollow knock with a creak
            // Keep in step with FootstepSurfaces (Level/LevelData.h).
        };

        Atom::SoundHandle Wind(float seconds = 24.0f);
        Atom::SoundHandle CicadaBed(float seconds = 16.0f);
        Atom::SoundHandle Drone(float seconds = 20.0f);
        Atom::SoundHandle VendingHum();
        Atom::SoundHandle RadioStatic(float seconds = 6.0f);
        Atom::SoundHandle RoomTone(float seconds = 8.0f);

        // One-shots.
        Atom::SoundHandle Higurashi(std::uint32_t seed);
        Atom::SoundHandle Creak(); // windmill axle
        Atom::SoundHandle Footstep(Surface surface, std::uint32_t variant);
    }
}
