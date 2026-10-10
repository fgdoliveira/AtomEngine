#pragma once

// A live synthesiser on the audio device (M80): an SDL audio stream whose
// callback, on SDL's audio thread, lets a game's sequencer schedule notes
// and then renders the Synth. The game thread only posts small commands
// through a lock-free single-producer/single-consumer queue - no locks and
// no allocation on the audio thread.

#include "Audio/Synth.h"

#include <array>
#include <atomic>
#include <cstdint>

struct SDL_AudioStream;

namespace Atom
{
    // A message from the game to its sequencer (meaning is the game's).
    struct SynthCommand
    {
        int type = 0;
        int argument = 0;
        float value = 0.0f;
    };

    // The game's music, run on the audio thread.
    class SynthSequencer
    {
    public:
        virtual ~SynthSequencer() = default;
        // A command the game posted, applied before the next block renders.
        virtual void OnCommand(Synth& synth, const SynthCommand& command) = 0;
        // Schedule every note that starts before `until` (seconds, the
        // synth's clock). Called once per rendered block.
        virtual void Advance(Synth& synth, double until) = 0;
    };

    class SynthStream
    {
    public:
        static constexpr int SampleRate = 48000;
        static constexpr int Block = 256; // frames per sequencer step

        SynthStream() = default;
        ~SynthStream();
        SynthStream(const SynthStream&) = delete;
        SynthStream& operator=(const SynthStream&) = delete;

        // Opens the default playback device and starts the callback; the
        // sequencer must outlive the stream. False if there's no audio
        // (the game then runs silent).
        bool Start(SynthSequencer& sequencer);
        void Stop();
        bool IsRunning() const { return m_stream != nullptr; }
        // The player's volume (M90), on the device stream: 0..1.
        void SetGain(float gain);

        // Game thread: queue a command; false if the queue is full (dropped).
        bool Send(const SynthCommand& command);

        // Renders `frames` directly (no device): what the callback does,
        // for tests and offline capture. Not while the stream runs.
        void RenderOffline(SynthSequencer& sequencer, float* out, int frames);

    private:
        static void Callback(void* user, SDL_AudioStream* stream, int additional, int total);
        void Fill(SynthSequencer& sequencer, float* out, int frames);

        SDL_AudioStream* m_stream = nullptr;
        SynthSequencer* m_sequencer = nullptr;
        Synth m_synth{ static_cast<float>(SampleRate) };

        std::array<SynthCommand, 256> m_queue{};
        std::atomic<std::uint32_t> m_head{ 0 }; // written by the game thread
        std::atomic<std::uint32_t> m_tail{ 0 }; // written by the audio thread
    };
}
