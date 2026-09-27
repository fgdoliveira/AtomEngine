#include "SoundSynth.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <numbers>
#include <random>
#include <vector>

namespace AtomGame::SoundSynth
{
    namespace
    {
        constexpr float Rate = static_cast<float>(Atom::AudioSystem::SampleRate);
        constexpr float TwoPi = 2.0f * std::numbers::pi_v<float>;

        std::size_t Frames(float seconds)
        {
            return static_cast<std::size_t>(seconds * Rate);
        }

        // Uniform white noise in [-1, 1].
        struct Noise
        {
            explicit Noise(std::uint32_t seed) : engine(seed) {}
            float operator()() { return distribution(engine); }
            float Uniform() { return distribution(engine) * 0.5f + 0.5f; }

            std::mt19937 engine;
            std::uniform_real_distribution<float> distribution{ -1.0f, 1.0f };
        };

        struct OnePoleLowpass
        {
            float state = 0.0f;
            float Process(float input, float cutoffHz)
            {
                const float a = 1.0f - std::exp(-TwoPi * cutoffHz / Rate);
                state += (input - state) * a;
                return state;
            }
        };

        // RBJ cookbook biquad (bandpass: constant 0 dB peak gain).
        struct Biquad
        {
            float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
            float x1 = 0, x2 = 0, y1 = 0, y2 = 0;

            static Biquad Bandpass(float centerHz, float q)
            {
                const float w = TwoPi * centerHz / Rate;
                const float alpha = std::sin(w) / (2.0f * q);
                const float a0 = 1.0f + alpha;
                Biquad f;
                f.b0 = alpha / a0;
                f.b1 = 0.0f;
                f.b2 = -alpha / a0;
                f.a1 = -2.0f * std::cos(w) / a0;
                f.a2 = (1.0f - alpha) / a0;
                return f;
            }

            static Biquad Highpass(float cutoffHz, float q = 0.707f)
            {
                const float w = TwoPi * cutoffHz / Rate;
                const float alpha = std::sin(w) / (2.0f * q);
                const float c = std::cos(w);
                const float a0 = 1.0f + alpha;
                Biquad f;
                f.b0 = (1.0f + c) / 2.0f / a0;
                f.b1 = -(1.0f + c) / a0;
                f.b2 = (1.0f + c) / 2.0f / a0;
                f.a1 = -2.0f * c / a0;
                f.a2 = (1.0f - alpha) / a0;
                return f;
            }

            float Process(float x)
            {
                const float y = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
                x2 = x1; x1 = x;
                y2 = y1; y1 = y;
                return y;
            }
        };

        // Smoothly wandering value in [0, 1], for gusts and swells.
        struct Wander
        {
            Wander(std::uint32_t seed, float changesPerSecond)
                : noise(seed), rate(changesPerSecond) {}

            float Next()
            {
                if (--countdown <= 0)
                {
                    target = noise.Uniform();
                    countdown = static_cast<int>(Rate / rate);
                }
                return smoother.Process(target, rate * 0.5f);
            }

            Noise noise;
            float rate;
            float target = 0.5f;
            int countdown = 0;
            OnePoleLowpass smoother{ 0.5f };
        };

        // Crossfades the last `fadeSeconds` into the start and drops them,
        // so the buffer loops seamlessly.
        void MakeLoop(std::vector<float>& samples, float fadeSeconds)
        {
            const std::size_t fade = std::min(Frames(fadeSeconds), samples.size() / 2);
            const std::size_t length = samples.size() - fade;
            for (std::size_t i = 0; i < fade; ++i)
            {
                const float t = static_cast<float>(i) / static_cast<float>(fade);
                // Equal-power fade: the tail fades out as the head fades in.
                const float headGain = std::sin(t * std::numbers::pi_v<float> * 0.5f);
                const float tailGain = std::cos(t * std::numbers::pi_v<float> * 0.5f);
                samples[i] = samples[i] * headGain + samples[length + i] * tailGain;
            }
            samples.resize(length);
        }

        void Normalize(std::vector<float>& samples, float peak)
        {
            float maximum = 0.0f;
            for (const float sample : samples)
            {
                maximum = std::max(maximum, std::abs(sample));
            }
            if (maximum > 0.0f)
            {
                const float scale = peak / maximum;
                for (float& sample : samples)
                {
                    sample *= scale;
                }
            }
        }

        Atom::SoundHandle Finish(std::vector<float> samples)
        {
            auto buffer = std::make_shared<Atom::SoundBuffer>();
            buffer->samples = std::move(samples);
            return buffer;
        }
    }

    Atom::SoundHandle Wind(float seconds)
    {
        std::vector<float> out(Frames(seconds));
        Noise noise(101);
        Wander gust(102, 0.35f);
        Wander tone(103, 0.2f);
        OnePoleLowpass low1, low2;
        Biquad air = Biquad::Highpass(40.0f);

        for (float& sample : out)
        {
            const float g = gust.Next();
            const float cutoff = 180.0f + 520.0f * tone.Next() * g;
            float s = low2.Process(low1.Process(noise(), cutoff), cutoff * 1.3f);
            sample = air.Process(s) * (0.25f + 0.75f * g * g);
        }

        MakeLoop(out, 2.0f);
        Normalize(out, 0.8f);
        return Finish(std::move(out));
    }

    Atom::SoundHandle CicadaBed(float seconds)
    {
        // Two distant choruses: a dense sizzle and a pulsing buzz, each
        // swelling slowly, like a hillside of cicadas far off.
        std::vector<float> out(Frames(seconds));
        Noise noise(201);
        Biquad sizzle = Biquad::Bandpass(5200.0f, 3.0f);
        Biquad buzz = Biquad::Bandpass(4100.0f, 6.0f);
        Wander swellA(202, 0.25f);
        Wander swellB(203, 0.18f);

        for (std::size_t i = 0; i < out.size(); ++i)
        {
            const float t = static_cast<float>(i) / Rate;
            const float n = noise();

            const float pulse = std::pow(0.5f + 0.5f * std::sin(TwoPi * 37.0f * t), 3.0f);
            const float a = sizzle.Process(n) * (0.4f + 0.6f * swellA.Next());
            const float b = buzz.Process(n) * pulse * (0.3f + 0.7f * swellB.Next());
            out[i] = a * 0.6f + b;
        }

        MakeLoop(out, 1.5f);
        Normalize(out, 0.6f);
        return Finish(std::move(out));
    }

    Atom::SoundHandle Drone(float seconds)
    {
        // Low, slowly beating tones under a faint rumble: unease you feel
        // more than hear. Frequencies are chosen to fit whole cycles.
        std::vector<float> out(Frames(seconds));
        Noise noise(301);
        OnePoleLowpass rumble;
        const float partials[][2] = {
            { 55.0f, 0.5f }, { 55.35f, 0.45f }, { 82.4f, 0.25f }, { 110.9f, 0.12f },
        };

        for (std::size_t i = 0; i < out.size(); ++i)
        {
            const float t = static_cast<float>(i) / Rate;
            float s = rumble.Process(noise(), 70.0f) * 2.0f;
            for (const auto& partial : partials)
            {
                s += std::sin(TwoPi * partial[0] * t) * partial[1];
            }
            const float breath = 0.75f + 0.25f * std::sin(TwoPi * t / seconds * 2.0f);
            out[i] = s * breath;
        }

        MakeLoop(out, 3.0f);
        Normalize(out, 0.7f);
        return Finish(std::move(out));
    }

    Atom::SoundHandle VendingHum()
    {
        // Compressor hum at 120 Hz (60 Hz mains, as in western Japan) with
        // falling harmonics and a thin electronic whine. Two seconds holds
        // whole cycles of every partial, so it loops without a seam.
        std::vector<float> out(Frames(2.0f));
        Noise noise(401);
        OnePoleLowpass hiss;
        for (std::size_t i = 0; i < out.size(); ++i)
        {
            const float t = static_cast<float>(i) / Rate;
            float s = std::sin(TwoPi * 120.0f * t) * 0.6f
                + std::sin(TwoPi * 240.0f * t) * 0.3f
                + std::sin(TwoPi * 360.0f * t) * 0.12f
                + std::sin(TwoPi * 7800.0f * t) * 0.015f;
            s += hiss.Process(noise(), 900.0f) * 0.08f;
            out[i] = s;
        }
        Normalize(out, 0.7f);
        return Finish(std::move(out));
    }

    Atom::SoundHandle RadioStatic(float seconds)
    {
        // Hiss, crackle and a drifting whistle, like a pocket radio between
        // stations.
        std::vector<float> out(Frames(seconds));
        Noise noise(501);
        Biquad hiss = Biquad::Highpass(900.0f);
        Wander flutter(502, 6.0f);
        Wander drift(503, 0.4f);
        float whistlePhase = 0.0f;

        for (std::size_t i = 0; i < out.size(); ++i)
        {
            float s = hiss.Process(noise()) * (0.35f + 0.65f * flutter.Next());
            if (noise.Uniform() < 0.0015f)
            {
                s += noise() * 2.5f; // crackle
            }
            const float frequency = 1100.0f + 900.0f * drift.Next();
            whistlePhase += TwoPi * frequency / Rate;
            s += std::sin(whistlePhase) * 0.06f;
            out[i] = s;
        }

        MakeLoop(out, 0.5f);
        Normalize(out, 0.7f);
        return Finish(std::move(out));
    }

    Atom::SoundHandle Higurashi(std::uint32_t seed)
    {
        // The evening cicada's falling "kana-kana-kana": a train of short
        // trilled notes that slows, drops in pitch and fades.
        Noise noise(600 + seed);
        const int notes = 10 + static_cast<int>(noise.Uniform() * 6.0f);
        const float basePitch = 4300.0f + noise() * 300.0f;

        std::vector<float> out(Frames(0.2f * notes + 0.5f), 0.0f);
        float phase = 0.0f;
        float start = 0.0f;
        for (int note = 0; note < notes; ++note)
        {
            const float progress = static_cast<float>(note) / static_cast<float>(notes);
            const float length = 0.09f + 0.05f * progress;
            const float pitch = basePitch * (1.0f - 0.12f * progress);
            const float level = 1.0f - 0.7f * progress;

            const std::size_t first = Frames(start);
            const std::size_t count = Frames(length);
            for (std::size_t i = 0; i < count && first + i < out.size(); ++i)
            {
                const float t = static_cast<float>(i) / Rate;
                const float envelope = std::sin(std::numbers::pi_v<float> * t / length);
                const float trill = 0.5f + 0.5f * std::sin(TwoPi * 95.0f * t);
                phase += TwoPi * pitch / Rate;
                out[first + i] += std::sin(phase) * envelope * trill * level;
            }
            start += length + 0.05f + 0.04f * progress;
        }

        Normalize(out, 0.6f);
        return Finish(std::move(out));
    }

    Atom::SoundHandle Footstep(Surface surface, std::uint32_t variant)
    {
        Noise noise(700 + static_cast<std::uint32_t>(surface) * 31 + variant);
        const float pitch = 0.9f + 0.2f * noise.Uniform();
        std::vector<float> out;

        switch (surface)
        {
        case Surface::Asphalt:
        case Surface::Concrete:
        {
            // Rubber sole on hard ground: a dull thud plus a short scuff.
            const bool concrete = surface == Surface::Concrete;
            out.resize(Frames(0.12f));
            OnePoleLowpass scuff;
            for (std::size_t i = 0; i < out.size(); ++i)
            {
                const float t = static_cast<float>(i) / Rate;
                const float thud = std::sin(TwoPi * 85.0f * pitch * t) * std::exp(-t / 0.018f);
                const float grit = scuff.Process(noise(), concrete ? 2600.0f : 1400.0f)
                    * std::exp(-t / (concrete ? 0.02f : 0.03f));
                out[i] = thud * 0.8f + grit * 2.2f;
            }
            break;
        }
        case Surface::Dirt:
        {
            // Packed earth and gravel: soft body with scattered crunches.
            out.resize(Frames(0.22f));
            Biquad crunch = Biquad::Bandpass(2400.0f * pitch, 1.2f);
            OnePoleLowpass body;
            for (std::size_t i = 0; i < out.size(); ++i)
            {
                const float t = static_cast<float>(i) / Rate;
                const float decay = std::exp(-t / 0.06f);
                float grains = noise.Uniform() < 0.02f * decay ? noise() * 4.0f : 0.0f;
                out[i] = crunch.Process(grains + noise() * 0.3f) * decay * 1.5f
                    + body.Process(noise(), 500.0f) * std::exp(-t / 0.04f) * 1.2f;
            }
            break;
        }
        case Surface::Stone:
        {
            // Hard, bright click with a short knock.
            out.resize(Frames(0.09f));
            Biquad click = Biquad::Bandpass(3000.0f * pitch, 2.0f);
            for (std::size_t i = 0; i < out.size(); ++i)
            {
                const float t = static_cast<float>(i) / Rate;
                const float knock = std::sin(TwoPi * 420.0f * pitch * t) * std::exp(-t / 0.012f);
                out[i] = click.Process(noise()) * std::exp(-t / 0.01f) * 2.0f + knock * 0.6f;
            }
            break;
        }
        }

        Normalize(out, 0.8f);
        return Finish(std::move(out));
    }
}
