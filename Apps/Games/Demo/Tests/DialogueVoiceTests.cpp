#include "Dialogue/DialogueVoice.h"
#include "Dialogue/Dialogue.h"
#include "Dialogue/DialogueRunner.h"
#include "TestAssets.h" // the owner's assets, then Content/
#include "World/GameState.h"

#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <memory>
#include <sstream>
#include <vector>

using namespace Demo;

namespace
{
    std::vector<VoicePreset> ShippedVoices()
    {
        std::ifstream file(AtomTests::Asset("Data/voices.json"), std::ios::binary);
        REQUIRE(file);
        std::stringstream text;
        text << file.rdbuf();
        std::vector<VoicePreset> voices;
        const std::string error = ParseVoices(text.str(), voices);
        INFO(error);
        REQUIRE(error.empty());
        return voices;
    }

    Dialogue OneLine(const std::string& text)
    {
        Dialogue dialogue;
        dialogue.id = "test";
        dialogue.start = "a";
        dialogue.voice = "man";
        DialogueNode node;
        node.id = "a";
        node.text = text;
        dialogue.nodes.emplace("a", node);
        return dialogue;
    }

    // Renders `seconds` of the speech's stream offline; returns the samples.
    std::vector<float> Render(DialogueSpeech& speech, float seconds)
    {
        std::vector<float> out(static_cast<std::size_t>(seconds * Atom::SynthStream::SampleRate));
        speech.Stream().RenderOffline(speech.Sequencer(), out.data(), static_cast<int>(out.size()));
        return out;
    }

    float Peak(const std::vector<float>& samples, std::size_t from = 0)
    {
        float peak = 0.0f;
        for (std::size_t i = from; i < samples.size(); ++i)
        {
            REQUIRE(std::isfinite(samples[i]));
            peak = std::max(peak, std::abs(samples[i]));
        }
        return peak;
    }

    // Runs the typewriter to the end, speaking as it goes, at 60 fps; with
    // `samples`, the audio renders alongside, a frame at a time (as the
    // device would), and is appended there.
    void Speak(DialogueRunner& runner, DialogueSpeech& speech, std::vector<float>* samples = nullptr)
    {
        for (int frame = 0; frame < 6000 && runner.GetState() == DialogueRunner::State::Revealing; ++frame)
        {
            runner.Update(1.0f / 60.0f);
            speech.Update(runner);
            if (samples)
            {
                const std::vector<float> block = Render(speech, 1.0f / 60.0f);
                samples->insert(samples->end(), block.begin(), block.end());
            }
        }
        speech.Update(runner);
    }
}

TEST_CASE("The shipped voices parse: old_man, sinister and man")
{
    const std::vector<VoicePreset> voices = ShippedVoices();
    for (const char* name : { "old_man", "sinister", "man" })
    {
        CHECK(std::any_of(voices.begin(), voices.end(), [&](const VoicePreset& v) { return v.name == name; }));
    }
}

TEST_CASE("Every shipped dialogue names a voice that exists")
{
    const std::vector<VoicePreset> voices = ShippedVoices();
    DialogueLibrary library;
    REQUIRE(library.LoadDirectory(AtomTests::Asset("Dialogue")) > 0);
    const Dialogue* keeper = library.Find("shrine_keeper");
    REQUIRE(keeper != nullptr);
    CHECK(keeper->voice == "old_man");
    const auto known = [&](const std::string& name) {
        return name.empty() || std::any_of(voices.begin(), voices.end(), [&](const VoicePreset& v) { return v.name == name; });
    };
    CHECK(known(keeper->voice));
    for (const auto& [id, node] : keeper->nodes)
    {
        CHECK(known(node.voice));
    }
}

TEST_CASE("Broken voices are rejected with their place")
{
    std::vector<VoicePreset> voices;
    CHECK_FALSE(ParseVoices("{ not json", voices).empty());
    CHECK(ParseVoices(R"({ "voices": {} })", voices).find("/voices") != std::string::npos);
    CHECK(ParseVoices(R"({ "voices": { "a": { "pitch": 5 } } })", voices).find("/voices/a/pitch") != std::string::npos);
    CHECK(ParseVoices(R"({ "voices": { "a": { "waveform": "noise" } } })", voices).find("/voices/a/waveform")
          != std::string::npos);
    CHECK(ParseVoices(R"({ "voices": { "a": { "pitch": 200 } } })", voices).empty());
    REQUIRE(voices.size() == 1);
    CHECK(voices[0].pitch == doctest::Approx(200.0f));
}

TEST_CASE("Letters and digits speak; spaces and punctuation don't")
{
    for (const char c : std::string("aZ09"))
    {
        CHECK(Voiced(static_cast<unsigned char>(c)));
    }
    for (const char c : std::string(" \n.,!?'-\""))
    {
        CHECK_FALSE(Voiced(static_cast<unsigned char>(c)));
    }
    CHECK(Voiced(0xC3)); // the lead byte of é
    CHECK_FALSE(Voiced(0xA9)); // a continuation byte
    CHECK_FALSE(Voiced(0xE2)); // an ellipsis or a dash
}

TEST_CASE("A syllable is deterministic and stays inside its voice")
{
    for (const VoicePreset& voice : ShippedVoices())
    {
        INFO(voice.name);
        for (int index = 0; index < 500; ++index)
        {
            const std::uint32_t letter = 'a' + static_cast<std::uint32_t>(index % 26);
            const Syllable s = MakeSyllable(voice, letter, index);
            const Syllable again = MakeSyllable(voice, letter, index);
            CHECK(s.frequency == again.frequency);
            CHECK(s.duration == again.duration);
            CHECK(s.frequency >= voice.pitch * (1.0f - voice.pitchSpread) - 0.01f);
            CHECK(s.frequency <= voice.pitch * (1.0f + voice.pitchSpread) + 0.01f);
            CHECK(s.duration >= voice.syllableSeconds * 0.85f - 1e-4f);
            CHECK(s.duration <= voice.syllableSeconds * 1.15f + 1e-4f);
            CHECK(s.volume <= voice.volume + 1e-6f);
            CHECK(s.breathVolume <= voice.breath * voice.volume + 1e-6f);
        }
    }
}

TEST_CASE("The typewriter rests after punctuation, and skipping ends the rest")
{
    CHECK(DialogueRunner::PauseAfter(",") == doctest::Approx(0.12f));
    CHECK(DialogueRunner::PauseAfter("?") == doctest::Approx(0.3f));
    CHECK(DialogueRunner::PauseAfter("\xE2\x80\xA6") == doctest::Approx(0.3f));
    CHECK(DialogueRunner::PauseAfter("a") == 0.0f);

    const Dialogue dialogue = OneLine("Ab. Cd");
    AtomFramework::GameState state;
    DialogueRunner runner;
    runner.charactersPerSecond = 100.0f; // everything in one frame, but for the stop
    runner.Start(dialogue, state);
    runner.Update(0.1f);
    CHECK(runner.RevealedCount() == 3); // "Ab." then a breath
    runner.Update(0.2f);
    CHECK(runner.RevealedCount() == 3); // still resting
    runner.Update(0.2f);
    CHECK(runner.GetState() == DialogueRunner::State::WaitingForInput);
    CHECK_FALSE(runner.WasSkipped());

    runner.Start(dialogue, state);
    runner.Update(0.1f);
    runner.Advance(); // mid-rest
    CHECK(runner.GetState() == DialogueRunner::State::WaitingForInput);
    CHECK(runner.RevealedCount() == 6);
    CHECK(runner.WasSkipped());
}

TEST_CASE("A spoken line plays syllables, finite and below full scale")
{
    auto speech = std::make_unique<DialogueSpeech>(); // the synth's delay line is 256 KB
    speech->Sequencer().SetPresets(ShippedVoices());
    const Dialogue dialogue = OneLine("You walk this road in the fog? Few do, these days.");
    AtomFramework::GameState state;
    DialogueRunner runner;
    runner.Start(dialogue, state);
    std::vector<float> samples;
    Speak(runner, *speech, &samples);

    // "man": one syllable every 2 of the 37 voiced letters.
    CHECK(speech->Sent() == 19);
    CHECK(speech->Dropped() == 0);
    const std::vector<float> tail = Render(*speech, 0.5f);
    samples.insert(samples.end(), tail.begin(), tail.end());
    CHECK(speech->Sequencer().Played() == 19);
    CHECK(speech->Sequencer().TooSoon() == 0); // at 45 letters a second, never too soon
    const float peak = Peak(samples);
    CHECK(peak > 0.01f);
    CHECK(peak < 1.0f);
}

TEST_CASE("Every voice is loud and bright enough for laptop speakers")
{
    // The first presets measured -40 dBFS RMS of mostly bass: silent on a
    // laptop under the street's rain. A floor on the level (RMS over the
    // spoken line) and on the highs (RMS of the sample-to-sample change).
    for (const VoicePreset& voice : ShippedVoices())
    {
        INFO(voice.name);
        auto speech = std::make_unique<DialogueSpeech>();
        speech->Sequencer().SetPresets({ voice });
        const Dialogue dialogue = OneLine("You walk this road in the fog? Few do, these days.");
        AtomFramework::GameState state;
        DialogueRunner runner;
        runner.Start(dialogue, state);
        std::vector<float> samples;
        Speak(runner, *speech, &samples);
        double level = 0.0;
        double highs = 0.0;
        for (std::size_t i = 1; i < samples.size(); ++i)
        {
            level += samples[i] * samples[i];
            highs += (samples[i] - samples[i - 1]) * (samples[i] - samples[i - 1]);
        }
        CHECK(std::sqrt(level / samples.size()) > 0.03);  // about -30 dBFS
        CHECK(std::sqrt(highs / samples.size()) > 0.001);
        CHECK(Peak(samples) < 0.7f);
    }
}

TEST_CASE("Empty and very long lines are safe")
{
    auto speech = std::make_unique<DialogueSpeech>();
    speech->Sequencer().SetPresets(ShippedVoices());
    AtomFramework::GameState state;
    DialogueRunner runner;

    const Dialogue empty = OneLine("");
    runner.Start(empty, state);
    Speak(runner, *speech);
    CHECK(speech->Sent() == 0);

    // 10k letters and no punctuation: at most one syllable a frame, and a
    // full queue drops syllables rather than blocking.
    const Dialogue longLine = OneLine(std::string(10000, 'a'));
    runner.charactersPerSecond = 2000.0f;
    runner.Start(longLine, state);
    Speak(runner, *speech);
    CHECK(speech->Sent() + speech->Dropped() <= 300 + 2);
    CHECK(speech->Sent() <= 256); // the queue's size: nothing rendered it meanwhile
    CHECK(Peak(Render(*speech, 1.0f)) < 1.0f);
}

TEST_CASE("Skipping or closing a line cuts the voice within 50 ms")
{
    auto speech = std::make_unique<DialogueSpeech>();
    std::vector<VoicePreset> voices = ShippedVoices();
    for (VoicePreset& v : voices)
    {
        v.syllableSeconds = 0.5f; // long tails, so a cut is audible
        v.echo = 0.0f;
    }
    speech->Sequencer().SetPresets(voices);
    const Dialogue dialogue = OneLine("Hmmmm hmmmm hmmmm hmmmm");
    AtomFramework::GameState state;
    DialogueRunner runner;
    runner.Start(dialogue, state);
    for (int frame = 0; frame < 10; ++frame)
    {
        runner.Update(1.0f / 60.0f);
        speech->Update(runner);
    }
    REQUIRE(speech->Sent() > 0);
    CHECK(Peak(Render(*speech, 0.05f)) > 0.01f); // sounding

    runner.Advance(); // skip
    speech->Update(runner);
    const std::vector<float> after = Render(*speech, 0.1f);
    CHECK(Peak(after, after.size() / 2) < 0.002f); // silent by 50 ms

    // Closing works the same way.
    runner.Start(dialogue, state);
    for (int frame = 0; frame < 10; ++frame)
    {
        runner.Update(1.0f / 60.0f);
        speech->Update(runner);
    }
    CHECK(Peak(Render(*speech, 0.05f)) > 0.01f);
    runner.Close();
    speech->Update(runner);
    const std::vector<float> closed = Render(*speech, 0.1f);
    CHECK(Peak(closed, closed.size() / 2) < 0.002f);
}

TEST_CASE("A flood of syllables saturates the voices without harm")
{
    auto speech = std::make_unique<DialogueSpeech>();
    speech->Sequencer().SetPresets(ShippedVoices());
    Atom::SynthStream& stream = speech->Stream();
    // A backlog: nothing rendered while 1000 syllables were posted.
    int accepted = 0;
    for (int i = 0; i < 1000; ++i)
    {
        accepted += stream.Send({ VoiceSequencer::Say, 'a' + i % 26, static_cast<float>(i) }) ? 1 : 0;
    }
    CHECK(accepted == 256); // the queue's size; the rest refused, not blocked
    const float peak = Peak(Render(*speech, 0.5f)); // finite (Peak checks)
    CHECK(peak < 1.0f);
    CHECK(speech->Sequencer().Played() == 1); // the backlog arrived at once: one syllable
    CHECK(speech->Sequencer().TooSoon() == 255);
}
