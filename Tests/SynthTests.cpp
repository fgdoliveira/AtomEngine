// The engine's synthesiser (M80) and DRIFT's soundtrack on it, rendered
// offline and measured.
#include "Audio/Synth.h"
#include "Audio/SynthStream.h"
#include "Music.h"

#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>

using namespace Atom;
using doctest::Approx;

namespace
{
    std::vector<float> Render(Synth& synth, float seconds)
    {
        std::vector<float> out(static_cast<std::size_t>(seconds * synth.SampleRate()));
        for (std::size_t done = 0; done < out.size(); done += 256)
        {
            synth.Render(out.data() + done, static_cast<int>(std::min<std::size_t>(256, out.size() - done)));
        }
        return out;
    }

    float Rms(const std::vector<float>& s, std::size_t from, std::size_t to)
    {
        double sum = 0.0;
        for (std::size_t i = from; i < to; ++i)
        {
            sum += static_cast<double>(s[i]) * s[i];
        }
        return static_cast<float>(std::sqrt(sum / static_cast<double>(to - from)));
    }

    SynthNote Tone(Waveform waveform, float frequency, float duration, SynthBus bus)
    {
        SynthNote n;
        n.waveform = waveform;
        n.frequency = frequency;
        n.duration = duration;
        n.volume = 0.5f;
        n.attack = 0.01f;
        n.bus = bus;
        return n;
    }
}

TEST_CASE("Synth: a sine voice plays at its frequency")
{
    auto synth = std::make_unique<Synth>(48000.0f);
    synth->Play(Tone(Waveform::Sine, 440.0f, 2.0f, SynthBus::Master));
    const std::vector<float> s = Render(*synth, 1.0f);
    int crossings = 0;
    for (std::size_t i = 1; i < s.size(); ++i)
    {
        crossings += (s[i - 1] < 0.0f) != (s[i] < 0.0f) ? 1 : 0;
    }
    CHECK(crossings == doctest::Approx(880).epsilon(0.01)); // two per cycle
}

TEST_CASE("Synth: linear attack to the peak, exponential decay to 0.0001 at the duration")
{
    auto synth = std::make_unique<Synth>(48000.0f);
    SynthNote n = Tone(Waveform::Sine, 1000.0f, 1.0f, SynthBus::Master);
    n.attack = 0.1f;
    synth->Play(n);
    const std::vector<float> s = Render(*synth, 1.2f);
    const float early = Rms(s, 480, 1440);      // 10-30 ms: rising
    const float peak = Rms(s, 4800, 6240);      // 100-130 ms: at the top
    const float late = Rms(s, 43200, 47520);    // 0.9-0.99 s: nearly gone
    // A sine's RMS (0.5 / sqrt 2), times the master gain (0.7, the
    // original's), times the decay already under way 0-30 ms after the
    // attack (to 0.0001 over 0.9 s: RMS factor sqrt((1 - e^-2kT) / 2kT)
    // with k = ln(5000) / 0.9, T = 0.03 s - about 0.875).
    const double k = std::log(5000.0) / 0.9;
    const double decay = std::sqrt((1.0 - std::exp(-2.0 * k * 0.03)) / (2.0 * k * 0.03));
    CHECK(peak == Approx(0.5 / std::sqrt(2.0) * 0.7 * decay).epsilon(0.03));
    CHECK(early < peak * 0.4f);
    CHECK(late < peak * 0.01f);
    CHECK(Rms(s, 53000, 57600) == 0.0f); // stopped after duration + 50 ms
    CHECK(synth->ActiveVoices() == 0);
}

TEST_CASE("Synth: the filtered bus's low-pass attenuates what's above its cutoff")
{
    auto open = std::make_unique<Synth>(48000.0f);
    open->SetFilterTarget(20000.0f, 0.001f);
    open->Play(Tone(Waveform::Sine, 6000.0f, 2.0f, SynthBus::Filtered));
    auto shut = std::make_unique<Synth>(48000.0f); // 600 Hz, as the original starts
    shut->Play(Tone(Waveform::Sine, 6000.0f, 2.0f, SynthBus::Filtered));
    const std::vector<float> a = Render(*open, 0.5f);
    const std::vector<float> b = Render(*shut, 0.5f);
    CHECK(Rms(b, 4800, 24000) < Rms(a, 4800, 24000) * 0.05f); // ~ -40 dB two octaves up
}

TEST_CASE("Synth: the delay repeats the effects bus after its delay time")
{
    auto synth = std::make_unique<Synth>(48000.0f);
    synth->SetDelay(0.46875f, 0.35f, 0.3f); // 0.75 beat at 96 BPM
    SynthNote blip = Tone(Waveform::Sine, 800.0f, 0.03f, SynthBus::Effects);
    synth->Play(blip);
    const std::vector<float> s = Render(*synth, 1.0f);
    const std::size_t echo = static_cast<std::size_t>(0.46875f * 48000.0f);
    CHECK(Rms(s, 9600, 19200) < 1e-6f);               // silence between
    CHECK(Rms(s, echo, echo + 1200) > 0.01f);          // the repeat
}

TEST_CASE("Drift music: eighth notes at 96 BPM, from 0.1 s")
{
    auto synth = std::make_unique<Synth>(48000.0f);
    Drift::Music music(1);
    music.Advance(*synth, 10.0);
    // Steps at 0.1 + k * 0.3125 s below 10 s: k = 0..31.
    CHECK(music.Step() == 32);
    CHECK(music.NextStepTime() == Approx(0.1 + 32 * 0.3125));
}

TEST_CASE("Drift music: flow opens the filter toward 500 + flow^2 * 6000 Hz")
{
    auto stream = std::make_unique<SynthStream>();
    Drift::Music music(1);
    stream->Send({ static_cast<int>(Drift::MusicCommand::Flow), 0, 0.5f });
    std::vector<float> s(48000 * 3);
    stream->RenderOffline(music, s.data(), static_cast<int>(s.size()));
    CHECK(music.Flow() == Approx(0.5f));
}

TEST_CASE("Drift music: four seconds of the soundtrack are audible and sane")
{
    auto stream = std::make_unique<SynthStream>();
    Drift::Music music(7);
    stream->Send({ static_cast<int>(Drift::MusicCommand::Flow), 0, 0.6f });
    stream->Send({ static_cast<int>(Drift::MusicCommand::Chime), 0, 0.0f });
    stream->Send({ static_cast<int>(Drift::MusicCommand::Pickup), 3, 0.0f });
    std::vector<float> s(48000 * 4);
    stream->RenderOffline(music, s.data(), static_cast<int>(s.size()));
    bool finite = true;
    float peak = 0.0f;
    for (float v : s)
    {
        finite = finite && std::isfinite(v);
        peak = std::max(peak, std::abs(v));
    }
    CHECK(finite);
    CHECK(peak < 1.0f);                      // no clipping
    CHECK(Rms(s, 0, s.size()) > 0.01f);      // music, not silence
}
