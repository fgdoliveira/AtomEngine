#include "SoundSynth.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <numbers>
#include <random>
#include <vector>

namespace Demo::SoundSynth
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

    Atom::SoundHandle Rain(float seconds)
    {
        // A wash of filtered noise - drops too many to hear one by one -
        // with sparse nearer ticks on top, swelling slightly.
        std::vector<float> out(Frames(seconds));
        Noise noise(301);
        Noise ticks(302);
        Wander swell(303, 0.3f);
        OnePoleLowpass low;
        Biquad high = Biquad::Highpass(350.0f);
        Biquad tickBand = Biquad::Bandpass(3200.0f, 1.5f);
        float tick = 0.0f;
        for (float& sample : out)
        {
            const float wash = high.Process(low.Process(noise(), 4200.0f));
            if (ticks() > 0.9985f)
            {
                tick = 1.0f; // a drop close by
            }
            tick *= 0.996f;
            const float drop = tickBand.Process(noise()) * tick;
            sample = wash * (0.75f + 0.25f * swell.Next()) + drop * 0.8f;
        }
        MakeLoop(out, 1.0f);
        Normalize(out, 0.7f);
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

    Atom::SoundHandle Creak()
    {
        // A wooden axle groaning under load: a low stick-slip tone (a
        // falling pitch chopped into pulses) over a body resonance.
        std::vector<float> out(Frames(0.7f));
        Noise noise(820);
        Biquad body = Biquad::Bandpass(240.0f, 3.0f);
        float phase = 0.0f;
        for (std::size_t i = 0; i < out.size(); ++i)
        {
            const float t = static_cast<float>(i) / Rate;
            const float envelope = std::clamp(t / 0.06f, 0.0f, 1.0f) * std::exp(-t / 0.25f);
            phase += TwoPi * (190.0f - 60.0f * t) / Rate;
            const float slip = 0.5f + 0.5f * std::sin(TwoPi * 23.0f * t + 3.0f * std::sin(TwoPi * 3.0f * t));
            const float tone = std::sin(phase) + 0.4f * std::sin(2.0f * phase + 0.7f);
            out[i] = (tone * slip + body.Process(noise()) * 1.5f) * envelope;
        }
        Normalize(out, 0.7f);
        return Finish(std::move(out));
    }

    Atom::SoundHandle RoomTone(float seconds)
    {
        // A closed room: the street's wind heard through walls (a heavy
        // low-pass), and a slow clock somewhere in the house.
        std::vector<float> out(Frames(seconds));
        Noise noise(801);
        Wander gust(802, 0.3f);
        OnePoleLowpass wall1, wall2;
        Biquad tick = Biquad::Bandpass(2600.0f, 3.0f);
        for (std::size_t i = 0; i < out.size(); ++i)
        {
            const float t = static_cast<float>(i) / Rate;
            float s = wall2.Process(wall1.Process(noise(), 90.0f), 120.0f) * (0.5f + 0.5f * gust.Next()) * 6.0f;
            const float sinceTick = std::fmod(t, 1.0f);
            s += tick.Process(noise() * std::exp(-sinceTick / 0.004f)) * 0.9f;
            out[i] = s;
        }
        MakeLoop(out, 0.5f);
        Normalize(out, 0.6f);
        return Finish(std::move(out));
    }

    Atom::SoundHandle Traffic(float seconds)
    {
        // Roads a few blocks away: a low roar that swells as cars pass,
        // with the odd far horn.
        std::vector<float> out(Frames(seconds));
        Noise noise(901);
        Wander passing(902, 0.5f);
        Wander distant(903, 0.12f);
        OnePoleLowpass road1, road2;
        Biquad air = Biquad::Highpass(30.0f);
        float hornPhase = 0.0f;
        for (std::size_t i = 0; i < out.size(); ++i)
        {
            const float t = static_cast<float>(i) / Rate;
            const float swell = passing.Next();
            const float cutoff = 220.0f + 380.0f * swell * swell;
            float s = road2.Process(road1.Process(noise(), cutoff), cutoff) * (0.4f + 0.6f * distant.Next());
            s = air.Process(s) * (0.5f + swell);
            // Two short horn blasts, far off, at fixed points of the loop.
            for (const float at : { seconds * 0.3f, seconds * 0.72f })
            {
                const float since = t - at;
                if (since >= 0.0f && since < 0.45f)
                {
                    hornPhase += TwoPi * 392.0f / Rate;
                    const float envelope = std::sin(std::numbers::pi_v<float> * since / 0.45f);
                    s += (std::sin(hornPhase) + 0.5f * std::sin(hornPhase * 1.26f)) * envelope * 0.02f;
                }
            }
            out[i] = s;
        }
        MakeLoop(out, 2.0f);
        Normalize(out, 0.7f);
        return Finish(std::move(out));
    }

    Atom::SoundHandle NeonBuzz()
    {
        // Sign ballasts at 120 Hz with harsh odd harmonics, and the dry
        // sizzle of an old tube. Two seconds of whole cycles: seamless.
        std::vector<float> out(Frames(2.0f));
        Noise noise(911);
        Biquad sizzle = Biquad::Bandpass(3800.0f, 1.5f);
        for (std::size_t i = 0; i < out.size(); ++i)
        {
            const float t = static_cast<float>(i) / Rate;
            float s = std::sin(TwoPi * 120.0f * t) * 0.45f
                + std::sin(TwoPi * 360.0f * t) * 0.25f
                + std::sin(TwoPi * 600.0f * t) * 0.12f
                + std::sin(TwoPi * 840.0f * t) * 0.06f;
            const float crackle = std::pow(0.5f + 0.5f * std::sin(TwoPi * 120.0f * t), 12.0f);
            s += sizzle.Process(noise()) * (0.2f + crackle);
            out[i] = s;
        }
        Normalize(out, 0.6f);
        return Finish(std::move(out));
    }

    Atom::SoundHandle Voices(float seconds)
    {
        // A crowd behind a wall: voice-band noise chopped into syllables
        // by slow random envelopes, low-passed so no word comes through.
        std::vector<float> out(Frames(seconds));
        Noise noise(921);
        Biquad formantA = Biquad::Bandpass(520.0f, 2.5f);
        Biquad formantB = Biquad::Bandpass(1150.0f, 3.0f);
        Wander syllablesA(922, 5.0f);
        Wander syllablesB(923, 3.5f);
        Wander mood(924, 0.15f);
        OnePoleLowpass wall;
        for (std::size_t i = 0; i < out.size(); ++i)
        {
            const float n = noise();
            const float a = formantA.Process(n) * std::pow(syllablesA.Next(), 2.0f);
            const float b = formantB.Process(n) * std::pow(syllablesB.Next(), 2.0f);
            out[i] = wall.Process((a + b * 0.7f) * (0.5f + mood.Next()), 1400.0f);
        }
        MakeLoop(out, 1.5f);
        Normalize(out, 0.6f);
        return Finish(std::move(out));
    }

    Atom::SoundHandle StreetBells(float seconds)
    {
        // Bicycle bells ringing now and then up the street: two metal
        // partials, inharmonic, with a fast strike and a long ring.
        std::vector<float> out(Frames(seconds), 0.0f);
        const float rings[][2] = { { 2.0f, 1.0f }, { 2.25f, 0.8f }, { 9.5f, 0.6f }, { 14.0f, 0.9f }, { 14.2f, 0.7f } };
        for (const auto& ring : rings)
        {
            const std::size_t first = Frames(ring[0]);
            for (std::size_t i = 0; i < Frames(1.2f) && first + i < out.size(); ++i)
            {
                const float t = static_cast<float>(i) / Rate;
                const float envelope = std::exp(-t / 0.35f) * std::clamp(t / 0.002f, 0.0f, 1.0f);
                out[first + i] += (std::sin(TwoPi * 2350.0f * t) + 0.6f * std::sin(TwoPi * 3170.0f * t)
                    + 0.3f * std::sin(TwoPi * 5230.0f * t)) * envelope * ring[1];
            }
        }
        Normalize(out, 0.5f);
        return Finish(std::move(out));
    }

    Atom::SoundHandle PachinkoLeak(float seconds)
    {
        // A pachinko hall heard through glass doors: the roar of steel
        // balls (dense bright clicks), a pounding march under it, all
        // muffled - the hall itself (M27) is where it gets loud.
        std::vector<float> out(Frames(seconds));
        Noise noise(931);
        Biquad balls = Biquad::Bandpass(3200.0f, 1.2f);
        Wander surge(932, 0.4f);
        OnePoleLowpass door1, door2;
        const float notes[] = { 262.0f, 330.0f, 392.0f, 330.0f, 294.0f, 349.0f, 440.0f, 349.0f };
        for (std::size_t i = 0; i < out.size(); ++i)
        {
            const float t = static_cast<float>(i) / Rate;
            const float click = noise.Uniform() < 0.08f ? noise() * 3.0f : 0.0f;
            float s = balls.Process(click + noise() * 0.2f) * (0.6f + 0.4f * surge.Next());
            const float beat = std::fmod(t, 0.5f);
            const float note = notes[static_cast<std::size_t>(t / 0.5f) % 8];
            s += std::sin(TwoPi * note * t) * std::exp(-beat / 0.2f) * 0.5f;
            s += std::sin(TwoPi * 55.0f * t) * std::exp(-beat / 0.05f) * 0.8f;
            out[i] = door2.Process(door1.Process(s, 900.0f), 1200.0f);
        }
        MakeLoop(out, 0.5f);
        Normalize(out, 0.7f);
        return Finish(std::move(out));
    }

    Atom::SoundHandle Train(float seconds)
    {
        // Wheels on rail: a heavy rumble, a steel whine, and the
        // da-dum of the rail joints (two bogies, twice a second).
        std::vector<float> out(Frames(seconds));
        Noise noise(941);
        OnePoleLowpass rumble1, rumble2;
        Biquad whine = Biquad::Bandpass(1900.0f, 8.0f);
        Biquad joint = Biquad::Bandpass(160.0f, 2.0f);
        for (std::size_t i = 0; i < out.size(); ++i)
        {
            const float t = static_cast<float>(i) / Rate;
            float s = rumble2.Process(rumble1.Process(noise(), 90.0f), 140.0f) * 8.0f;
            s += whine.Process(noise()) * 0.3f;
            const float cycle = std::fmod(t, 0.5f);
            const float hit = std::exp(-cycle / 0.02f) + std::exp(-std::max(cycle - 0.09f, 0.0f) / 0.02f) * (cycle > 0.09f);
            s += joint.Process(noise() * hit) * 6.0f;
            out[i] = s;
        }
        MakeLoop(out, 0.25f);
        Normalize(out, 0.8f);
        return Finish(std::move(out));
    }

    Atom::SoundHandle BusEngine()
    {
        // A diesel at a fast idle: firing pulses at 30 Hz (whole cycles in
        // two seconds, so it loops seamlessly) through a boomy body, a
        // rattle on top, and the fan's hiss.
        std::vector<float> out(Frames(2.0f));
        Noise noise(951);
        Biquad body = Biquad::Bandpass(95.0f, 1.5f);
        Biquad rattle = Biquad::Bandpass(700.0f, 3.0f);
        OnePoleLowpass fan;
        for (std::size_t i = 0; i < out.size(); ++i)
        {
            const float t = static_cast<float>(i) / Rate;
            const float cycle = std::fmod(t * 30.0f, 1.0f);
            const float pulse = std::exp(-cycle / 0.12f);
            float s = body.Process(pulse * 2.0f + noise() * 0.1f) * 3.0f;
            s += std::sin(TwoPi * 60.0f * t) * 0.25f;
            s += rattle.Process(noise() * pulse) * 0.6f;
            s += fan.Process(noise(), 1500.0f) * 0.15f;
            out[i] = s;
        }
        Normalize(out, 0.8f);
        return Finish(std::move(out));
    }

    Atom::SoundHandle DoorHiss()
    {
        // Compressed air let out as the doors fold: a sharp hiss that
        // falls away, and the thunk of the doors at the end.
        std::vector<float> out(Frames(1.4f));
        Noise noise(961);
        Biquad air = Biquad::Highpass(2500.0f);
        Biquad thunk = Biquad::Bandpass(140.0f, 2.0f);
        for (std::size_t i = 0; i < out.size(); ++i)
        {
            const float t = static_cast<float>(i) / Rate;
            const float hiss = std::clamp(t / 0.02f, 0.0f, 1.0f) * std::exp(-t / 0.35f);
            float s = air.Process(noise()) * hiss;
            const float since = t - 0.9f;
            if (since > 0.0f)
            {
                s += thunk.Process(noise() * std::exp(-since / 0.01f)) * 8.0f;
            }
            out[i] = s;
        }
        Normalize(out, 0.8f);
        return Finish(std::move(out));
    }

    Atom::SoundHandle PachinkoHall(float seconds)
    {
        // Inside the hall: the steel-ball roar at full brightness, a march
        // from every machine a little out of step, and a shrill jingle when
        // someone somewhere wins. The level's reverb makes it a room.
        std::vector<float> out(Frames(seconds));
        Noise noise(971);
        Biquad balls = Biquad::Bandpass(3600.0f, 0.9f);
        Biquad chime = Biquad::Bandpass(5200.0f, 4.0f);
        Wander surge(972, 0.6f);
        const float notes[] = { 262.0f, 330.0f, 392.0f, 330.0f, 294.0f, 349.0f, 440.0f, 349.0f };
        for (std::size_t i = 0; i < out.size(); ++i)
        {
            const float t = static_cast<float>(i) / Rate;
            const float click = noise.Uniform() < 0.25f ? noise() * 2.0f : 0.0f;
            float s = balls.Process(click + noise() * 0.3f) * (0.7f + 0.3f * surge.Next()) * 1.4f;
            for (const float offset : { 0.0f, 0.13f, 0.31f })
            {
                const float local = t + offset;
                const float beat = std::fmod(local, 0.5f);
                const float note = notes[static_cast<std::size_t>(local / 0.5f) % 8] * (1.0f + offset * 0.1f);
                s += std::sin(TwoPi * note * t) * std::exp(-beat / 0.18f) * 0.22f;
            }
            const float since = std::fmod(t, seconds / 2.0f) - 3.0f;
            if (since > 0.0f && since < 1.2f)
            {
                s += chime.Process(noise()) * 0.4f + std::sin(TwoPi * (1568.0f + 400.0f * std::floor(since * 8.0f)) * t) * 0.12f;
            }
            out[i] = s;
        }
        MakeLoop(out, 0.5f);
        Normalize(out, 0.75f);
        return Finish(std::move(out));
    }

    Atom::SoundHandle BallClick()
    {
        // Steel on brass: two short, bright, inharmonic partials.
        std::vector<float> out(Frames(0.05f));
        for (std::size_t i = 0; i < out.size(); ++i)
        {
            const float t = static_cast<float>(i) / Rate;
            const float envelope = std::exp(-t / 0.006f);
            out[i] = (std::sin(TwoPi * 4200.0f * t) + 0.6f * std::sin(TwoPi * 6900.0f * t)) * envelope;
        }
        Normalize(out, 0.7f);
        return Finish(std::move(out));
    }

    Atom::SoundHandle PocketChime()
    {
        // Two quick bell notes, a fifth apart.
        std::vector<float> out(Frames(0.5f), 0.0f);
        for (const auto& [start, pitch] : { std::pair{ 0.0f, 1318.5f }, std::pair{ 0.08f, 1975.5f } })
        {
            for (std::size_t i = Frames(start); i < out.size(); ++i)
            {
                const float t = static_cast<float>(i) / Rate - start;
                out[i] += (std::sin(TwoPi * pitch * t) + 0.3f * std::sin(TwoPi * pitch * 2.76f * t)) * std::exp(-t / 0.12f);
            }
        }
        Normalize(out, 0.6f);
        return Finish(std::move(out));
    }

    Atom::SoundHandle Payout()
    {
        // A cascade of balls into a tin tray: many clicks, thinning out.
        std::vector<float> out(Frames(0.7f), 0.0f);
        Noise noise(981);
        Biquad tin = Biquad::Bandpass(3100.0f, 2.5f);
        for (std::size_t i = 0; i < out.size(); ++i)
        {
            const float t = static_cast<float>(i) / Rate;
            const float density = 0.02f * std::exp(-t / 0.3f);
            const float click = noise.Uniform() < density ? noise() * 3.0f : 0.0f;
            out[i] = tin.Process(click);
        }
        Normalize(out, 0.6f);
        return Finish(std::move(out));
    }

    Atom::SoundHandle ReelStop()
    {
        // A mechanical clunk with an electronic blip on top.
        std::vector<float> out(Frames(0.15f));
        Noise noise(991);
        Biquad body = Biquad::Bandpass(220.0f, 2.0f);
        for (std::size_t i = 0; i < out.size(); ++i)
        {
            const float t = static_cast<float>(i) / Rate;
            out[i] = body.Process(noise() * std::exp(-t / 0.01f)) * 4.0f
                + std::sin(TwoPi * 880.0f * t) * std::exp(-t / 0.04f) * 0.5f;
        }
        Normalize(out, 0.6f);
        return Finish(std::move(out));
    }

    Atom::SoundHandle SwitchClick()
    {
        // Two tiny knocks a few milliseconds apart: plastic sliding home.
        std::vector<float> out(Frames(0.06f));
        Noise noise(1234);
        Biquad tick = Biquad::Bandpass(3200.0f, 3.0f);
        for (std::size_t i = 0; i < out.size(); ++i)
        {
            const float t = static_cast<float>(i) / Rate;
            const float first = std::exp(-t / 0.002f);
            const float second = t > 0.012f ? std::exp(-(t - 0.012f) / 0.003f) * 0.6f : 0.0f;
            out[i] = tick.Process(noise() * (first + second));
        }
        Normalize(out, 0.45f);
        return Finish(std::move(out));
    }

    Atom::SoundHandle Reach()
    {
        // A rising, wobbling square-ish tone: the tease.
        std::vector<float> out(Frames(1.6f));
        float phase = 0.0f;
        for (std::size_t i = 0; i < out.size(); ++i)
        {
            const float t = static_cast<float>(i) / Rate;
            const float pitch = 440.0f * std::pow(2.0f, t / 1.6f) * (1.0f + 0.02f * std::sin(TwoPi * 7.0f * t));
            phase += TwoPi * pitch / Rate;
            const float square = std::sin(phase) + std::sin(3.0f * phase) / 3.0f + std::sin(5.0f * phase) / 5.0f;
            out[i] = square * std::min(1.0f, t / 0.05f) * (1.0f - 0.3f * t / 1.6f);
        }
        Normalize(out, 0.45f);
        return Finish(std::move(out));
    }

    Atom::SoundHandle Fanfare()
    {
        // A short major arpeggio and a held chord: the fever.
        std::vector<float> out(Frames(2.0f), 0.0f);
        const float notes[] = { 523.3f, 659.3f, 784.0f, 1046.5f };
        for (int n = 0; n < 4; ++n)
        {
            const float start = 0.12f * n;
            const float length = 2.0f - start;
            for (std::size_t i = Frames(start); i < out.size(); ++i)
            {
                const float t = static_cast<float>(i) / Rate - start;
                const float envelope = std::min(1.0f, t / 0.01f) * std::exp(-t / (n == 3 ? 0.9f : 0.5f));
                const float wave = std::sin(TwoPi * notes[n] * t) + 0.4f * std::sin(TwoPi * notes[n] * 2.0f * t);
                out[i] += wave * envelope * (t < length ? 1.0f : 0.0f);
            }
        }
        Normalize(out, 0.6f);
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
        case Surface::Wood:
        {
            // Hollow floorboard: a low resonant knock, then a faint creak.
            out.resize(Frames(0.28f));
            Biquad body = Biquad::Bandpass(180.0f * pitch, 4.0f);
            for (std::size_t i = 0; i < out.size(); ++i)
            {
                const float t = static_cast<float>(i) / Rate;
                const float knock = body.Process(noise() * std::exp(-t / 0.008f)) * 6.0f
                    + std::sin(TwoPi * 110.0f * pitch * t) * std::exp(-t / 0.05f) * 0.5f;
                const float creakEnvelope = std::clamp((t - 0.06f) / 0.04f, 0.0f, 1.0f) * std::exp(-(t - 0.06f) / 0.07f);
                const float creak = std::sin(TwoPi * (620.0f + 180.0f * t) * pitch * t)
                    * (0.5f + 0.5f * std::sin(TwoPi * 37.0f * t)) * creakEnvelope * 0.25f;
                out[i] = knock + creak;
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
