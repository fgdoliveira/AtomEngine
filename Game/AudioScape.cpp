#include "AudioScape.h"

#include "Scene/Camera.h"

#include <glm/gtc/constants.hpp>

#include <cmath>
#include <iostream>

namespace AtomGame
{
    namespace
    {
        // Vending machines in street.glb (glTF space: Blender Y -> -Z).
        constexpr glm::vec3 VendingMachines[] = {
            { -20.0f, 1.0f, -4.1f },
            { 29.8f, 1.0f, -4.1f },
            { -2.6f, 1.0f, 4.1f },
        };
    }

    void AudioScape::Initialize(Atom::AudioSystem& audio)
    {
        m_audio = &audio;
        if (!audio.IsAvailable())
        {
            return;
        }

        // Beds: everywhere, unpositioned.
        Atom::PlayParams bed{};
        bed.loop = true;

        bed.gain = 0.55f;
        audio.Play(SoundSynth::Wind(), bed);
        bed.gain = 0.18f;
        audio.Play(SoundSynth::CicadaBed(), bed);
        bed.gain = 0.16f;
        audio.Play(SoundSynth::Drone(), bed);

        // Vending machines hum where they stand.
        const Atom::SoundHandle hum = SoundSynth::VendingHum();
        for (const glm::vec3& position : VendingMachines)
        {
            Atom::PlayParams params{};
            params.loop = true;
            params.gain = 0.22f;
            params.spatial = true;
            params.position = position;
            params.minDistance = 1.5f;
            params.maxDistance = 14.0f;
            m_humVoices.push_back(audio.Play(hum, params));
        }

        for (int surface = 0; surface < 4; ++surface)
        {
            for (int variant = 0; variant < FootstepVariants; ++variant)
            {
                m_footsteps[surface][variant] = SoundSynth::Footstep(
                    static_cast<SoundSynth::Surface>(surface),
                    static_cast<std::uint32_t>(variant)
                );
            }
        }

        for (std::uint32_t seed = 0; seed < 3; ++seed)
        {
            m_higurashi.push_back(SoundSynth::Higurashi(seed));
        }

        // Static loops silently until an unease beat raises it.
        m_static = SoundSynth::RadioStatic();
        Atom::PlayParams staticParams{};
        staticParams.loop = true;
        staticParams.gain = 0.0f;
        m_staticVoice = audio.Play(m_static, staticParams);

        std::cout << "AudioScape: sounds synthesised\n";
    }

    SoundSynth::Surface AudioScape::SurfaceAt(const glm::vec3& position)
    {
        using SoundSynth::Surface;

        // Shrine approach (stone path behind the torii).
        if (position.x > -14.8f && position.x < -13.2f
            && position.z > 4.9f && position.z < 10.3f)
        {
            return Surface::Stone;
        }

        const float side = std::abs(position.z);
        if (side < 3.0f)
        {
            return Surface::Asphalt;
        }
        if (side < 3.65f)
        {
            return Surface::Concrete; // gutter and curb
        }
        return Surface::Dirt;
    }

    void AudioScape::Update(
        float deltaSeconds,
        const Atom::Camera& camera,
        const Listener& listener
    )
    {
        if (!m_audio || !m_audio->IsAvailable())
        {
            return;
        }

        m_audio->SetListener(camera.GetPosition(), camera.GetFlatRight());

        // One footfall per head-bob cycle.
        if (listener.stepCount != m_lastStep)
        {
            m_lastStep = listener.stepCount;
            if (listener.grounded)
            {
                const auto surface = static_cast<int>(SurfaceAt(listener.feetPosition));
                std::uniform_int_distribution<int> pick(0, FootstepVariants - 1);
                std::uniform_real_distribution<float> vary(0.94f, 1.06f);

                Atom::PlayParams step{};
                step.gain = (listener.jogging ? 0.55f : 0.38f) * vary(m_random);
                step.pitch = vary(m_random);
                m_audio->Play(m_footsteps[surface][pick(m_random)], step);
            }
        }

        // Now and then an evening cicada calls from somewhere in the fog.
        m_higurashiTimer -= deltaSeconds;
        if (m_higurashiTimer <= 0.0f)
        {
            std::uniform_real_distribution<float> unit(0.0f, 1.0f);
            m_higurashiTimer = 14.0f + 22.0f * unit(m_random);

            const float angle = glm::two_pi<float>() * unit(m_random);
            const float distance = 25.0f + 20.0f * unit(m_random);

            Atom::PlayParams call{};
            call.spatial = true;
            call.gain = 0.35f;
            call.pitch = 0.95f + 0.1f * unit(m_random);
            call.minDistance = 10.0f;
            call.maxDistance = 90.0f;
            call.position = listener.feetPosition
                + glm::vec3{ std::cos(angle) * distance, 6.0f, std::sin(angle) * distance };
            const auto index = static_cast<std::size_t>(unit(m_random) * m_higurashi.size())
                % m_higurashi.size();
            m_audio->Play(m_higurashi[index], call);
        }
    }

    void AudioScape::SetStaticLevel(float level)
    {
        if (m_audio && m_staticVoice)
        {
            m_audio->SetVoiceGain(m_staticVoice, 0.45f * level);
        }
    }

    void AudioScape::SetHumLevel(float level)
    {
        if (!m_audio)
        {
            return;
        }
        for (const Atom::VoiceId voice : m_humVoices)
        {
            m_audio->SetVoiceGain(voice, 0.22f * level);
        }
    }

    void AudioScape::ToggleMute()
    {
        if (!m_audio)
        {
            return;
        }
        m_muted = !m_muted;
        m_audio->SetMasterGain(m_muted ? 0.0f : 0.8f);
    }
}
