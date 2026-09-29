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

        // The night city (M25).
        Atom::SoundHandle Traffic(float seconds = 20.0f);       // distant roads, swelling
        Atom::SoundHandle NeonBuzz();                           // ballast hum, sizzling tubes
        Atom::SoundHandle Voices(float seconds = 16.0f);        // murmur from a bar, muffled
        Atom::SoundHandle StreetBells(float seconds = 18.0f);   // bicycle bells now and then
        Atom::SoundHandle PachinkoLeak(float seconds = 12.0f);  // the hall through its doors
        Atom::SoundHandle Train(float seconds = 6.0f);          // wheels and rail joints
        Atom::SoundHandle BusEngine();                          // M26: a diesel idling, looped
        Atom::SoundHandle DoorHiss();                           // M26: air doors folding open

        // One-shots.
        Atom::SoundHandle Higurashi(std::uint32_t seed);
        Atom::SoundHandle Creak(); // windmill axle
        Atom::SoundHandle Footstep(Surface surface, std::uint32_t variant);
    }
}
