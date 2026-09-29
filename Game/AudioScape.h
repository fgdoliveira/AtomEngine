#pragma once

#include "Audio/AudioSystem.h"
#include "SoundSynth.h"

#include <glm/vec3.hpp>

#include <array>
#include <functional>
#include <random>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace Atom
{
    class Camera;
}

namespace AtomGame
{
    // The persistent part of the soundscape. It owns the sound library
    // (named, synthesised once) and the sounds that follow the player
    // everywhere: footsteps, distant cicada calls, radio static. Levels start
    // their own ambience from the library by name and stop it on unload.
    class AudioScape
    {
    public:
        void Initialize(Atom::AudioSystem& audio);

        // Named sounds for level files: "wind", "cicadas", "drone",
        // "vending_hum", "static", "room_tone", "creak"; the night city's
        // "traffic", "neon_buzz", "voices", "street_bells",
        // "pachinko_leak", "train"; the bus's "bus_engine", "door_hiss".
        Atom::SoundHandle GetSound(std::string_view name) const;

        struct Listener
        {
            glm::vec3 feetPosition{ 0.0f };
            int stepCount = 0;  // increments once per footfall
            bool grounded = true;
            bool jogging = false;
        };

        void Update(
            float deltaSeconds,
            const Atom::Camera& camera,
            const Listener& listener
        );

        // Where am I standing? Supplied by the current level ("asphalt",
        // "dirt", "stone", "concrete", "wood").
        void SetSurfaceProvider(std::function<std::string_view(float x, float z)> provider)
        {
            m_surfaceAt = std::move(provider);
        }
        void SetOutdoor(bool outdoor) { m_outdoor = outdoor; }

        // 0..1; radio static for the unease beats.
        void SetStaticLevel(float level);

        void ToggleMute();
        bool IsMuted() const { return m_muted; }

    private:
        SoundSynth::Surface SurfaceUnderfoot(const glm::vec3& position) const;

        Atom::AudioSystem* m_audio = nullptr;
        std::unordered_map<std::string, Atom::SoundHandle> m_library;

        static constexpr int SurfaceCount = 5;
        static constexpr int FootstepVariants = 4;
        std::array<std::array<Atom::SoundHandle, FootstepVariants>, SurfaceCount> m_footsteps;
        std::vector<Atom::SoundHandle> m_higurashi;

        std::function<std::string_view(float, float)> m_surfaceAt;
        Atom::VoiceId m_staticVoice = 0;
        int m_lastStep = 0;
        float m_higurashiTimer = 6.0f;
        bool m_outdoor = true;
        bool m_muted = false;

        std::mt19937 m_random{ 12345 };
    };
}
