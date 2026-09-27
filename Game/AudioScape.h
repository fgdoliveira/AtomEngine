#pragma once

#include "Audio/AudioSystem.h"
#include "SoundSynth.h"

#include <glm/vec3.hpp>

#include <array>
#include <random>
#include <vector>

namespace Atom
{
    class Camera;
}

namespace AtomGame
{
    // The demo's soundscape: ambience beds, vending machine hums, footsteps
    // per surface and distant evening-cicada calls.
    class AudioScape
    {
    public:
        void Initialize(Atom::AudioSystem& audio);

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

        // 0..1; radio static for the unease beats (M8.3).
        void SetStaticLevel(float level);

        void ToggleMute();
        bool IsMuted() const { return m_muted; }

    private:
        static SoundSynth::Surface SurfaceAt(const glm::vec3& position);

        Atom::AudioSystem* m_audio = nullptr;

        static constexpr int FootstepVariants = 4;
        std::array<std::array<Atom::SoundHandle, FootstepVariants>, 4> m_footsteps;
        std::vector<Atom::SoundHandle> m_higurashi;
        Atom::SoundHandle m_static;

        Atom::VoiceId m_staticVoice = 0;
        int m_lastStep = 0;
        float m_higurashiTimer = 6.0f;
        bool m_muted = false;

        std::mt19937 m_random{ 12345 };
    };
}
