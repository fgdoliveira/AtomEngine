// The Showcase's synth booth (v0.0.14), rendered offline: keys in, sound out.
#include "BoothSynth.h"

#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>

using namespace Showcase;

namespace
{
    constexpr int Rate = Atom::SynthStream::SampleRate;

    float Rms(const std::vector<float>& s, double from, double to)
    {
        const std::size_t a = static_cast<std::size_t>(from * Rate);
        const std::size_t b = std::min(s.size(), static_cast<std::size_t>(to * Rate));
        double sum = 0.0;
        for (std::size_t i = a; i < b; ++i)
        {
            sum += static_cast<double>(s[i]) * s[i];
        }
        return static_cast<float>(std::sqrt(sum / static_cast<double>(b - a)));
    }

    // `seconds` of the booth after the given commands (applied before the
    // first block, as the audio thread applies queued ones).
    std::vector<float> Play(std::initializer_list<Atom::SynthCommand> commands, float seconds)
    {
        auto stream = std::make_unique<Atom::SynthStream>(); // heap: the delay line is large
        BoothSynth booth;
        for (const Atom::SynthCommand& command : commands)
        {
            stream->Send(command);
        }
        std::vector<float> out(static_cast<std::size_t>(seconds * Rate));
        stream->RenderOffline(booth, out.data(), static_cast<int>(out.size()));
        return out;
    }

    Atom::SynthCommand Note(int key) { return { static_cast<int>(BoothCommand::Note), key, 0.0f }; }
    Atom::SynthCommand Delay(bool on) { return { static_cast<int>(BoothCommand::Delay), on ? 1 : 0, 0.0f }; }
}

TEST_CASE("Booth synth: silent until a key, then an audible, unclipped note")
{
    const std::vector<float> idle = Play({}, 0.5f);
    CHECK(Rms(idle, 0.0, 0.5) < 1e-6f);

    const std::vector<float> note = Play({ Note(0) }, 1.0f);
    CHECK(Rms(note, 0.0, 0.3) > 0.01f);
    const float peak = std::abs(*std::max_element(note.begin(), note.end(),
                                                  [](float a, float b) { return std::abs(a) < std::abs(b); }));
    CHECK(peak < 1.0f);
    CHECK(std::all_of(note.begin(), note.end(), [](float x) { return std::isfinite(x); }));
}

TEST_CASE("Booth synth: the delay repeats a note after it has faded")
{
    // The note's envelope is done by 1.6 s; only the echo can sound later.
    const std::vector<float> dry = Play({ Note(2) }, 2.6f);
    const std::vector<float> wet = Play({ Delay(true), Note(2) }, 2.6f);
    CHECK(Rms(dry, 1.9, 2.6) < 1e-4f);
    CHECK(Rms(wet, 1.9, 2.6) > 10.0f * Rms(dry, 1.9, 2.6) + 1e-4f);
}
