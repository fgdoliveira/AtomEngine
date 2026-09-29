#pragma once

#include "Audio/Reverb.h"

#include <glm/vec3.hpp>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

struct SDL_AudioStream;

namespace Atom
{
    // Mono float PCM at AudioSystem::SampleRate.
    struct SoundBuffer
    {
        std::vector<float> samples;
    };

    using SoundHandle = std::shared_ptr<const SoundBuffer>;
    using VoiceId = std::uint32_t;

    struct PlayParams
    {
        float gain = 1.0f;
        float pitch = 1.0f;   // playback rate multiplier
        bool loop = false;

        // Spatial voices attenuate with distance from the listener and pan
        // left/right; non-spatial voices play centred.
        bool spatial = false;
        glm::vec3 position{ 0.0f };
        float minDistance = 1.0f;  // full volume inside this radius
        float maxDistance = 30.0f; // silent beyond this radius
    };

    // Small software mixer on an SDL audio stream: stereo float output,
    // voices are mono buffers. The mixer runs on SDL's audio thread; every
    // public call locks the stream, so it is safe from the game thread.
    class AudioSystem
    {
    public:
        static constexpr int SampleRate = 48000;

        AudioSystem() = default;
        ~AudioSystem();

        AudioSystem(const AudioSystem&) = delete;
        AudioSystem& operator=(const AudioSystem&) = delete;

        // Returns false if no audio device is available; every other call
        // then becomes a no-op so the game still runs silently.
        bool Initialize();
        void Shutdown();
        bool IsAvailable() const { return m_stream != nullptr; }

        // Loads any WAV SDL understands, converted to mono float at SampleRate.
        static SoundHandle LoadWav(const std::string& path);

        VoiceId Play(const SoundHandle& sound, const PlayParams& params);
        void Stop(VoiceId voice);
        void SetVoiceGain(VoiceId voice, float gain);
        void SetVoicePosition(VoiceId voice, const glm::vec3& position);

        void SetListener(
            const glm::vec3& position,
            const glm::vec3& right
        );
        void SetMasterGain(float gain);
        // Room reverb on the whole mix (M27); mix 0 turns it off.
        void SetReverb(float mix, float size, float feedback);
        float GetMasterGain() const { return m_masterGain; }
        std::size_t GetVoiceCount() const;
        // Looping voices only: the ones that play until stopped, so a leak
        // shows up here (one-shots end by themselves).
        std::size_t GetLoopingVoiceCount() const;

    private:
        struct Voice
        {
            VoiceId id = 0;
            SoundHandle sound;
            double cursor = 0.0;
            PlayParams params;

            // Per-channel gains the mixer ramps towards, recomputed from the
            // listener whenever anything changes.
            float targetLeft = 0.0f;
            float targetRight = 0.0f;
            float currentLeft = 0.0f;
            float currentRight = 0.0f;
        };

        static void MixCallback(
            void* userdata,
            SDL_AudioStream* stream,
            int additionalBytes,
            int totalBytes
        );
        void Mix(int frames);
        void UpdateGains(Voice& voice) const;
        Voice* FindVoice(VoiceId id);

        void WriteCapture() const;

        SDL_AudioStream* m_stream = nullptr;
        std::vector<Voice> m_voices;
        std::vector<float> m_mixBuffer;

        // Dev aid: ATOM_AUDIO_CAPTURE=<file.wav> records the first minute
        // of output and writes it on shutdown.
        std::string m_capturePath;
        std::vector<float> m_capture;
        VoiceId m_nextId = 1;

        glm::vec3 m_listenerPosition{ 0.0f };
        glm::vec3 m_listenerRight{ 1.0f, 0.0f, 0.0f };
        float m_masterGain = 0.8f;
        Reverb m_reverb;
    };
}
