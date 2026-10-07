#pragma once

// A small synthesiser (M80): the DSP a generative soundtrack needs, modelled
// on the WebAudio graph DRIFT's original builds - oscillator and noise
// voices with attack/exponential-decay envelopes, mixed into a filtered
// bus, an effects bus, a feedback delay and a master gain. Pure: it renders
// floats offline (unit tests) or inside an audio callback (SynthStream).
// Nothing allocates after construction: voices come from a fixed pool.

#include <array>
#include <cstdint>
#include <random>

namespace Atom
{
    enum class Waveform : std::uint8_t { Sine, Triangle, Sawtooth, Noise };
    enum class SynthBus : std::uint8_t { Filtered, Effects, Master };
    enum class NoiseFilter : std::uint8_t { None, Lowpass, Highpass };

    // One note, in the units WebAudio uses.
    struct SynthNote
    {
        Waveform waveform = Waveform::Sine;
        double start = 0.0;          // seconds, on the synth's clock
        float frequency = 440.0f;    // Hz (ignored by noise)
        float detuneCents = 0.0f;
        float duration = 0.5f;       // until the envelope reaches 0.0001
        float volume = 0.1f;         // peak gain
        float attack = 0.01f;        // linear rise; 0 = starts at the peak
        SynthBus bus = SynthBus::Filtered;
        // A pitch sweep (the kick): exponentially to sweepTo over sweepTime.
        float sweepTo = 0.0f;
        float sweepTime = 0.0f;
        // Noise only: its own biquad.
        NoiseFilter noiseFilter = NoiseFilter::None;
        float noiseCutoff = 1000.0f;
    };

    // The RBJ cookbook biquad. Q here is linear; WebAudio's low-pass and
    // high-pass Q is in dB (linear = 10^(dB/20)).
    class Biquad
    {
    public:
        void SetLowpass(float cutoff, float q, float sampleRate);
        void SetHighpass(float cutoff, float q, float sampleRate);
        float Process(float x);

    private:
        float m_b0 = 1.0f, m_b1 = 0.0f, m_b2 = 0.0f, m_a1 = 0.0f, m_a2 = 0.0f;
        float m_x1 = 0.0f, m_x2 = 0.0f, m_y1 = 0.0f, m_y2 = 0.0f;
    };

    class Synth
    {
    public:
        static constexpr int MaxVoices = 160;

        explicit Synth(float sampleRate = 48000.0f);

        float SampleRate() const { return m_sampleRate; }
        double Now() const { return static_cast<double>(m_frame) / m_sampleRate; } // seconds rendered

        // Queues a note; false if every voice is busy (the note is dropped).
        bool Play(const SynthNote& note);

        // The filtered bus's low-pass: a target cutoff approached with a
        // time constant (WebAudio setTargetAtTime), and its Q (linear).
        void SetFilterTarget(float cutoff, float timeConstant);
        void SetFilterQ(float q) { m_filterQ = q; }
        // The feedback delay: time (s), feedback, wet level.
        void SetDelay(float seconds, float feedback, float wet);
        // The master gain, approached with a time constant (mute).
        void SetMasterTarget(float gain, float timeConstant);
        float MasterTarget() const { return m_masterTarget; }

        // Renders mono samples, advancing the clock.
        void Render(float* out, int frames);

        int ActiveVoices() const;
        float FilterCutoff() const { return m_cutoff; }

    private:
        struct Voice
        {
            bool active = false;
            SynthNote note;
            double phase = 0.0;
            Biquad noise;
            // Exponential ramps as a per-sample ratio, not a pow() per sample.
            bool decaying = false;
            double envelope = 0.0;
            double decayRatio = 1.0;
            double frequency = 0.0;
            double sweepRatio = 1.0;
        };
        float VoiceSample(Voice& voice, double time);

        float m_sampleRate;
        std::uint64_t m_frame = 0;
        std::array<Voice, MaxVoices> m_voices{};

        Biquad m_filter;
        float m_cutoff = 600.0f;
        float m_cutoffTarget = 600.0f;
        float m_cutoffTimeConstant = 0.3f;
        float m_filterQ = 1.2589f; // 2 dB, the original's Q

        std::array<float, 1 << 16> m_delayLine{}; // ~1.36 s at 48 kHz
        int m_delayWrite = 0;
        int m_delaySamples = 0;
        float m_feedback = 0.0f;
        float m_wet = 0.0f;

        float m_master = 0.7f;
        float m_masterTarget = 0.7f;
        float m_masterTimeConstant = 0.1f;

        std::minstd_rand m_noise{ 12345 };
    };
}
