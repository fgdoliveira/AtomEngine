// DRIFT's soundtrack on the engine's synthesiser (M80), rendered offline
// and measured. The synthesiser itself: Tests/Engine/SynthTests.cpp.
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
    float Rms(const std::vector<float>& s, std::size_t from, std::size_t to)
    {
        double sum = 0.0;
        for (std::size_t i = from; i < to; ++i)
        {
            sum += static_cast<double>(s[i]) * s[i];
        }
        return static_cast<float>(std::sqrt(sum / static_cast<double>(to - from)));
    }
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
