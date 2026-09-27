#include "AudioScape.h"

#include "Scene/Camera.h"

#include <glm/gtc/constants.hpp>

#include <cmath>
#include <iostream>

namespace AtomGame
{
    void AudioScape::Initialize(Atom::AudioSystem& audio)
    {
        m_audio = &audio;
        if (!audio.IsAvailable())
        {
            return;
        }

        // The library levels draw from. Synthesised once; levels only start
        // and stop voices that play these buffers.
        m_library["wind"] = SoundSynth::Wind();
        m_library["cicadas"] = SoundSynth::CicadaBed();
        m_library["drone"] = SoundSynth::Drone();
        m_library["vending_hum"] = SoundSynth::VendingHum();
        m_library["static"] = SoundSynth::RadioStatic();
        m_library["room_tone"] = SoundSynth::RoomTone();

        for (int surface = 0; surface < SurfaceCount; ++surface)
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

        // Static follows the player between levels; silent until an unease
        // beat raises it.
        Atom::PlayParams staticParams{};
        staticParams.loop = true;
        staticParams.gain = 0.0f;
        m_staticVoice = audio.Play(m_library["static"], staticParams);

        std::cout << "AudioScape: " << m_library.size() << " library sounds synthesised\n";
    }

    Atom::SoundHandle AudioScape::GetSound(std::string_view name) const
    {
        const auto found = m_library.find(std::string(name));
        if (found == m_library.end())
        {
            if (m_audio && m_audio->IsAvailable())
            {
                std::cerr << "Unknown sound '" << name << "'\n";
            }
            return nullptr;
        }
        return found->second;
    }

    SoundSynth::Surface AudioScape::SurfaceUnderfoot(const glm::vec3& position) const
    {
        using SoundSynth::Surface;
        const std::string_view name = m_surfaceAt ? m_surfaceAt(position.x, position.z) : "dirt";
        if (name == "asphalt") { return Surface::Asphalt; }
        if (name == "concrete") { return Surface::Concrete; }
        if (name == "stone") { return Surface::Stone; }
        if (name == "wood") { return Surface::Wood; }
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
                const auto surface = static_cast<int>(SurfaceUnderfoot(listener.feetPosition));
                std::uniform_int_distribution<int> pick(0, FootstepVariants - 1);
                std::uniform_real_distribution<float> vary(0.94f, 1.06f);

                Atom::PlayParams step{};
                step.gain = (listener.jogging ? 0.55f : 0.38f) * vary(m_random);
                step.pitch = vary(m_random);
                m_audio->Play(m_footsteps[surface][pick(m_random)], step);
            }
        }

        // Now and then an evening cicada calls from somewhere in the fog.
        if (!m_outdoor)
        {
            return;
        }
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
