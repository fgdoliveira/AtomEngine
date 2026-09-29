#include "Audio/AudioSystem.h"

#include <SDL3/SDL.h>

#include <glm/geometric.hpp>

#include <algorithm>
#include <cmath>
#include <iostream>

namespace Atom
{
    namespace
    {
        // Gains ramp to their targets over ~10 ms to avoid clicks.
        constexpr float GainRampPerFrame = 1.0f / (0.010f * AudioSystem::SampleRate);

        struct StreamLock
        {
            explicit StreamLock(SDL_AudioStream* stream) : stream(stream)
            {
                if (stream)
                {
                    SDL_LockAudioStream(stream);
                }
            }
            ~StreamLock()
            {
                if (stream)
                {
                    SDL_UnlockAudioStream(stream);
                }
            }
            SDL_AudioStream* stream;
        };

        float Approach(float current, float target)
        {
            if (current < target)
            {
                return std::min(current + GainRampPerFrame, target);
            }
            return std::max(current - GainRampPerFrame, target);
        }
    }

    AudioSystem::~AudioSystem()
    {
        Shutdown();
    }

    bool AudioSystem::Initialize()
    {
        if (!SDL_InitSubSystem(SDL_INIT_AUDIO))
        {
            std::cerr << "Audio unavailable: " << SDL_GetError() << '\n';
            return false;
        }

        const SDL_AudioSpec spec{ SDL_AUDIO_F32, 2, SampleRate };
        m_stream = SDL_OpenAudioDeviceStream(
            SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,
            &spec,
            &AudioSystem::MixCallback,
            this
        );
        if (!m_stream)
        {
            std::cerr << "Failed to open audio device: " << SDL_GetError() << '\n';
            SDL_QuitSubSystem(SDL_INIT_AUDIO);
            return false;
        }

        if (const char* capture = SDL_getenv("ATOM_AUDIO_CAPTURE"))
        {
            m_capturePath = capture;
            m_capture.reserve(static_cast<std::size_t>(SampleRate) * 2 * 60);
        }

        SDL_ResumeAudioStreamDevice(m_stream);
        std::cout << "Audio: 48 kHz stereo float mixer ready\n";
        return true;
    }

    void AudioSystem::Shutdown()
    {
        if (m_stream)
        {
            // Destroying the stream stops the callback before returning.
            SDL_DestroyAudioStream(m_stream);
            m_stream = nullptr;
            SDL_QuitSubSystem(SDL_INIT_AUDIO);
            WriteCapture();
        }
        m_voices.clear();
        m_capture.clear();
    }

    void AudioSystem::WriteCapture() const
    {
        if (m_capturePath.empty() || m_capture.empty())
        {
            return;
        }

        SDL_IOStream* file = SDL_IOFromFile(m_capturePath.c_str(), "wb");
        if (!file)
        {
            std::cerr << "Failed to write audio capture: " << SDL_GetError() << '\n';
            return;
        }

        // Minimal RIFF/WAVE header, IEEE float stereo.
        const Uint32 dataBytes = static_cast<Uint32>(m_capture.size() * sizeof(float));
        const auto u32 = [&](Uint32 value) { SDL_WriteU32LE(file, value); };
        const auto u16 = [&](Uint16 value) { SDL_WriteU16LE(file, value); };
        SDL_WriteIO(file, "RIFF", 4);
        u32(36 + dataBytes);
        SDL_WriteIO(file, "WAVEfmt ", 8);
        u32(16);
        u16(3); // WAVE_FORMAT_IEEE_FLOAT
        u16(2);
        u32(SampleRate);
        u32(SampleRate * 2 * sizeof(float));
        u16(2 * sizeof(float));
        u16(32);
        SDL_WriteIO(file, "data", 4);
        u32(dataBytes);
        SDL_WriteIO(file, m_capture.data(), dataBytes);
        SDL_CloseIO(file);

        std::cout << "Audio capture written: " << m_capturePath << '\n';
    }

    SoundHandle AudioSystem::LoadWav(const std::string& path)
    {
        SDL_AudioSpec sourceSpec{};
        Uint8* sourceData = nullptr;
        Uint32 sourceLength = 0;
        if (!SDL_LoadWAV(path.c_str(), &sourceSpec, &sourceData, &sourceLength))
        {
            std::cerr << "Failed to load WAV '" << path << "': " << SDL_GetError() << '\n';
            return nullptr;
        }

        const SDL_AudioSpec targetSpec{ SDL_AUDIO_F32, 1, SampleRate };
        Uint8* converted = nullptr;
        int convertedLength = 0;
        const bool ok = SDL_ConvertAudioSamples(
            &sourceSpec,
            sourceData,
            static_cast<int>(sourceLength),
            &targetSpec,
            &converted,
            &convertedLength
        );
        SDL_free(sourceData);

        if (!ok)
        {
            std::cerr << "Failed to convert WAV '" << path << "': " << SDL_GetError() << '\n';
            return nullptr;
        }

        auto buffer = std::make_shared<SoundBuffer>();
        const auto* samples = reinterpret_cast<const float*>(converted);
        buffer->samples.assign(samples, samples + convertedLength / sizeof(float));
        SDL_free(converted);
        return buffer;
    }

    VoiceId AudioSystem::Play(const SoundHandle& sound, const PlayParams& params)
    {
        if (!m_stream || !sound || sound->samples.empty())
        {
            return 0;
        }

        const StreamLock lock(m_stream);

        Voice voice{};
        voice.id = m_nextId++;
        voice.sound = sound;
        voice.params = params;
        UpdateGains(voice);
        // One-shots start at full level (a ramp would soften their attack);
        // loops fade in over the ramp.
        if (!params.loop)
        {
            voice.currentLeft = voice.targetLeft;
            voice.currentRight = voice.targetRight;
        }
        m_voices.push_back(std::move(voice));
        return m_voices.back().id;
    }

    void AudioSystem::Stop(VoiceId id)
    {
        const StreamLock lock(m_stream);
        std::erase_if(m_voices, [id](const Voice& voice) { return voice.id == id; });
    }

    void AudioSystem::SetVoiceGain(VoiceId id, float gain)
    {
        const StreamLock lock(m_stream);
        if (Voice* voice = FindVoice(id))
        {
            voice->params.gain = gain;
            UpdateGains(*voice);
        }
    }

    void AudioSystem::SetVoicePosition(VoiceId id, const glm::vec3& position)
    {
        const StreamLock lock(m_stream);
        if (Voice* voice = FindVoice(id))
        {
            voice->params.position = position;
            UpdateGains(*voice);
        }
    }

    void AudioSystem::SetListener(const glm::vec3& position, const glm::vec3& right)
    {
        const StreamLock lock(m_stream);
        m_listenerPosition = position;
        m_listenerRight = right;
        for (Voice& voice : m_voices)
        {
            UpdateGains(voice);
        }
    }

    void AudioSystem::SetMasterGain(float gain)
    {
        const StreamLock lock(m_stream);
        m_masterGain = std::clamp(gain, 0.0f, 1.0f);
        for (Voice& voice : m_voices)
        {
            UpdateGains(voice);
        }
    }

    void AudioSystem::SetReverb(float mix, float size, float feedback)
    {
        if (!m_stream)
        {
            return;
        }
        const StreamLock lock(m_stream);
        m_reverb.Configure(mix, size, feedback, SampleRate);
    }

    std::size_t AudioSystem::GetVoiceCount() const
    {
        const StreamLock lock(m_stream);
        return m_voices.size();
    }

    std::size_t AudioSystem::GetLoopingVoiceCount() const
    {
        const StreamLock lock(m_stream);
        return static_cast<std::size_t>(std::count_if(m_voices.begin(), m_voices.end(),
            [](const Voice& voice) { return voice.params.loop; }));
    }

    AudioSystem::Voice* AudioSystem::FindVoice(VoiceId id)
    {
        for (Voice& voice : m_voices)
        {
            if (voice.id == id)
            {
                return &voice;
            }
        }
        return nullptr;
    }

    void AudioSystem::UpdateGains(Voice& voice) const
    {
        float gain = voice.params.gain * m_masterGain;
        float pan = 0.0f; // -1 left .. +1 right

        if (voice.params.spatial)
        {
            const glm::vec3 offset = voice.params.position - m_listenerPosition;
            const float distance = glm::length(offset);

            // Inverse-distance rolloff, faded to exactly zero at maxDistance.
            const float minDistance = std::max(voice.params.minDistance, 0.01f);
            const float rolloff = minDistance / std::max(distance, minDistance);
            const float edge = std::clamp(
                (voice.params.maxDistance - distance)
                    / (voice.params.maxDistance * 0.25f),
                0.0f,
                1.0f
            );
            gain *= rolloff * edge;

            if (distance > 0.001f)
            {
                // Soften panning up close so sources don't jump sides.
                const float closeness = std::clamp(distance / minDistance, 0.0f, 1.0f);
                pan = glm::dot(offset / distance, m_listenerRight) * 0.8f * closeness;
            }
        }

        // Constant-power pan law.
        const float angle = (pan + 1.0f) * 0.25f * 3.14159265f;
        voice.targetLeft = gain * std::cos(angle);
        voice.targetRight = gain * std::sin(angle);
    }

    void AudioSystem::MixCallback(
        void* userdata,
        SDL_AudioStream* stream,
        int additionalBytes,
        int /*totalBytes*/
    )
    {
        auto* self = static_cast<AudioSystem*>(userdata);
        const int frames = additionalBytes / static_cast<int>(2 * sizeof(float));
        if (frames <= 0)
        {
            return;
        }

        // SDL holds the stream lock while calling us.
        self->Mix(frames);
        SDL_PutAudioStreamData(
            stream,
            self->m_mixBuffer.data(),
            frames * static_cast<int>(2 * sizeof(float))
        );
    }

    void AudioSystem::Mix(int frames)
    {
        m_mixBuffer.assign(static_cast<std::size_t>(frames) * 2, 0.0f);

        for (Voice& voice : m_voices)
        {
            const std::vector<float>& samples = voice.sound->samples;
            const double length = static_cast<double>(samples.size());
            const double step = voice.params.pitch;

            for (int frame = 0; frame < frames; ++frame)
            {
                if (voice.cursor >= length)
                {
                    if (!voice.params.loop)
                    {
                        break;
                    }
                    voice.cursor = std::fmod(voice.cursor, length);
                }

                // Linear interpolation between neighbouring samples.
                const auto index = static_cast<std::size_t>(voice.cursor);
                const float fraction = static_cast<float>(voice.cursor - index);
                const std::size_t next = index + 1 < samples.size()
                    ? index + 1
                    : (voice.params.loop ? 0 : index);
                const float sample = samples[index] + (samples[next] - samples[index]) * fraction;

                voice.currentLeft = Approach(voice.currentLeft, voice.targetLeft);
                voice.currentRight = Approach(voice.currentRight, voice.targetRight);
                m_mixBuffer[frame * 2] += sample * voice.currentLeft;
                m_mixBuffer[frame * 2 + 1] += sample * voice.currentRight;

                voice.cursor += step;
            }
        }

        std::erase_if(m_voices, [](const Voice& voice) {
            return !voice.params.loop
                && voice.cursor >= static_cast<double>(voice.sound->samples.size());
        });

        if (m_reverb.IsActive())
        {
            for (int frame = 0; frame < frames; ++frame)
            {
                m_reverb.Process(m_mixBuffer[frame * 2], m_mixBuffer[frame * 2 + 1]);
            }
        }

        // Soft clip so stacked sounds saturate gently instead of wrapping.
        for (float& sample : m_mixBuffer)
        {
            sample = std::tanh(sample);
        }

        if (!m_capturePath.empty() && m_capture.size() < m_capture.capacity())
        {
            const std::size_t room = m_capture.capacity() - m_capture.size();
            const std::size_t count = std::min(room, m_mixBuffer.size());
            m_capture.insert(m_capture.end(), m_mixBuffer.begin(), m_mixBuffer.begin() + count);
        }
    }
}
