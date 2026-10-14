#include "Dialogue/DialogueVoice.h"
#include "Dialogue/DialogueRunner.h"

#include "Level/JsonText.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace Demo
{
    namespace
    {
        using Json = nlohmann::json;

        // A small integer hash (lowbias32): letter and place to a number,
        // the same on every run and every machine.
        std::uint32_t Hash(std::uint32_t x)
        {
            x ^= x >> 16;
            x *= 0x7feb352dU;
            x ^= x >> 15;
            x *= 0x846ca68bU;
            x ^= x >> 16;
            return x;
        }

        // 0..1 from a hash.
        float Unit(std::uint32_t h) { return static_cast<float>(h & 0xFFFFu) / 65535.0f; }

        std::string Number(const Json& object, const char* key, const std::string& at, float min, float max, float& out)
        {
            const auto found = object.find(key);
            if (found == object.end())
            {
                return {};
            }
            if (!found->is_number() || found->get<float>() < min || found->get<float>() > max)
            {
                char range[64];
                std::snprintf(range, sizeof(range), "a number in [%g, %g]", min, max);
                return at + "/" + key + ": must be " + range;
            }
            out = found->get<float>();
            return {};
        }
    }

    bool Voiced(unsigned char c)
    {
        const bool asciiLetterOrDigit = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
        // A UTF-8 lead byte of a 2-byte sequence: Latin letters with
        // accents (é, ñ). Wider sequences are punctuation and symbols here.
        const bool accented = c >= 0xC3 && c <= 0xC5;
        return asciiLetterOrDigit || accented;
    }

    Syllable MakeSyllable(const VoicePreset& preset, std::uint32_t letter, int index)
    {
        // Mostly the letter (a word's vowels keep their colour), a little
        // the place (repeated letters don't drone).
        const std::uint32_t h = Hash(letter * 2654435761U ^ Hash(static_cast<std::uint32_t>(index)));
        const float offset = (Unit(h) * 2.0f - 1.0f) * preset.pitchSpread; // -spread..+spread
        Syllable s;
        s.frequency = preset.pitch * (1.0f + offset);
        s.sweepTo = s.frequency * preset.glide;
        s.duration = preset.syllableSeconds * (0.85f + 0.3f * Unit(Hash(h)));          // +-15 %
        s.volume = preset.volume * (0.8f + 0.2f * Unit(Hash(h + 1)));                   // -20 %
        s.breathVolume = preset.breath * s.volume * Unit(Hash(h + 2));                  // some letters breathe
        return s;
    }

    std::string ParseVoices(std::string_view text, std::vector<VoicePreset>& voices)
    {
        Json root;
        if (std::string error = AtomFramework::ParseJsonText(text, root); !error.empty())
        {
            return error;
        }
        const auto list = root.is_object() ? root.find("voices") : root.end();
        if (list == root.end() || !list->is_object() || list->empty())
        {
            return "/voices: missing, or not an object of voices";
        }
        std::vector<VoicePreset> parsed;
        for (const auto& [name, item] : list->items())
        {
            const std::string at = "/voices/" + name;
            if (!item.is_object())
            {
                return at + ": must be an object";
            }
            VoicePreset v;
            v.name = name;
            if (const auto wave = item.find("waveform"); wave != item.end())
            {
                const std::string w = wave->is_string() ? wave->get<std::string>() : std::string{};
                if (w == "sine") v.waveform = Atom::Waveform::Sine;
                else if (w == "triangle") v.waveform = Atom::Waveform::Triangle;
                else if (w == "sawtooth") v.waveform = Atom::Waveform::Sawtooth;
                else return at + "/waveform: must be \"sine\", \"triangle\" or \"sawtooth\"";
            }
            float letters = static_cast<float>(v.lettersPerSyllable);
            for (const std::string& error : {
                     Number(item, "pitch", at, 40.0f, 1000.0f, v.pitch),
                     Number(item, "pitchSpread", at, 0.0f, 0.9f, v.pitchSpread),
                     Number(item, "glide", at, 0.25f, 4.0f, v.glide),
                     Number(item, "syllableSeconds", at, 0.02f, 0.5f, v.syllableSeconds),
                     Number(item, "volume", at, 0.0f, 0.5f, v.volume),
                     Number(item, "lettersPerSyllable", at, 1.0f, 8.0f, letters),
                     Number(item, "breath", at, 0.0f, 1.0f, v.breath),
                     Number(item, "breathCutoff", at, 100.0f, 12000.0f, v.breathCutoff),
                     Number(item, "cutoff", at, 100.0f, 12000.0f, v.cutoff),
                     Number(item, "echo", at, 0.0f, 0.8f, v.echo) })
            {
                if (!error.empty())
                {
                    return error;
                }
            }
            v.lettersPerSyllable = static_cast<int>(letters);
            parsed.push_back(std::move(v));
        }
        if (parsed.size() > static_cast<std::size_t>(VoiceSequencer::MaxVoices))
        {
            return "/voices: at most 8 voices";
        }
        voices = std::move(parsed);
        return {};
    }

    void VoiceSequencer::SetPresets(const std::vector<VoicePreset>& presets)
    {
        m_count = std::min(static_cast<int>(presets.size()), MaxVoices);
        for (int i = 0; i < m_count; ++i)
        {
            m_presets[static_cast<std::size_t>(i)] = presets[static_cast<std::size_t>(i)];
        }
        m_current = 0;
    }

    int VoiceSequencer::Find(std::string_view name) const
    {
        for (int i = 0; i < m_count; ++i)
        {
            if (m_presets[static_cast<std::size_t>(i)].name == name)
            {
                return i;
            }
        }
        return -1;
    }

    void VoiceSequencer::OnCommand(Atom::Synth& synth, const Atom::SynthCommand& command)
    {
        // Audio thread: no allocation - presets are only read.
        if (m_count == 0)
        {
            return;
        }
        switch (command.type)
        {
        case Select:
        {
            m_current = std::clamp(command.argument, 0, m_count - 1);
            const VoicePreset& v = m_presets[static_cast<std::size_t>(m_current)];
            synth.SetFilterTarget(v.cutoff, 0.01f);
            synth.SetFilterQ(0.9f);
            synth.SetDelay(0.19f, 0.35f, v.echo);
            break;
        }
        case Say:
        {
            const VoicePreset& v = m_presets[static_cast<std::size_t>(m_current)];
            // At most one syllable per quarter syllable: a backlog (a
            // stalled audio thread, a flood) arrives all at once and never
            // stacks voices into a clip; speech at any typing speed passes.
            if (synth.Now() - m_lastSay < 0.25 * v.syllableSeconds)
            {
                ++m_tooSoon;
                break;
            }
            m_lastSay = synth.Now();
            const Syllable s = MakeSyllable(v, static_cast<std::uint32_t>(command.argument), static_cast<int>(command.value));
            synth.SetMasterTarget(0.7f, 0.005f); // back up after a Stop
            Atom::SynthNote tone;
            tone.waveform = v.waveform;
            tone.start = synth.Now();
            tone.frequency = s.frequency;
            tone.sweepTo = s.sweepTo;
            tone.sweepTime = s.duration;
            tone.duration = s.duration;
            tone.volume = s.volume;
            tone.attack = 0.008f;
            if (synth.Play(tone))
            {
                ++m_played;
            }
            if (s.breathVolume > 0.001f)
            {
                Atom::SynthNote breath;
                breath.waveform = Atom::Waveform::Noise;
                breath.start = tone.start;
                breath.duration = s.duration * 0.5f;
                breath.volume = s.breathVolume;
                breath.attack = 0.002f;
                breath.noiseFilter = Atom::NoiseFilter::Highpass;
                breath.noiseCutoff = v.breathCutoff;
                synth.Play(breath); // a full pool drops it: the tone matters more
            }
            break;
        }
        case Stop:
            synth.SetMasterTarget(0.0f, 0.008f); // the tail is gone in ~30 ms
            break;
        default:
            break;
        }
    }

    void VoiceSequencer::Advance(Atom::Synth&, double)
    {
        // Nothing scheduled ahead: syllables play as they are posted.
    }

    bool DialogueSpeech::Start(const std::vector<VoicePreset>& presets)
    {
        Stop();
        m_sequencer.SetPresets(presets);
        m_running = m_stream.Start(m_sequencer);
        m_gain = -1.0f;
        return m_running;
    }

    void DialogueSpeech::Stop()
    {
        m_stream.Stop();
        m_running = false;
    }

    void DialogueSpeech::SetVoice(std::string_view name)
    {
        const int index = std::max(0, m_sequencer.Find(name));
        // Read-only on both threads: presets change only while stopped.
        const VoicePreset* preset = m_sequencer.Preset(index);
        m_lettersPerSyllable = preset ? std::max(1, preset->lettersPerSyllable) : 2;
        Send(VoiceSequencer::Select, index);
    }

    void DialogueSpeech::SetGain(float gain)
    {
        if (gain != m_gain)
        {
            m_gain = gain;
            m_stream.SetGain(gain);
        }
    }

    void DialogueSpeech::Send(int type, int argument, float value)
    {
        // Offline (tests) the stream isn't running but still takes commands.
        if (m_stream.Send({ type, argument, value }))
        {
            if (type == VoiceSequencer::Say)
            {
                ++m_sent;
            }
        }
        else
        {
            ++m_dropped; // a full queue: the text goes on regardless
        }
    }

    void DialogueSpeech::Update(const DialogueRunner& runner)
    {
        const DialogueNode* node = runner.IsActive() ? runner.GetNode() : nullptr;
        const int line = node ? runner.LineSerial() : -1;
        if (line != m_line)
        {
            // A new line (or none): cut what's still sounding.
            if (m_revealing)
            {
                Send(VoiceSequencer::Stop);
            }
            m_line = line;
            m_spoken = 0;
            m_voiced = 0;
            m_revealing = node != nullptr;
            if (node)
            {
                const Dialogue* dialogue = runner.GetDialogue();
                SetVoice(!node->voice.empty() ? node->voice : dialogue ? dialogue->voice : std::string{});
            }
        }
        if (!node || !m_revealing)
        {
            return;
        }
        if (runner.WasSkipped())
        {
            Send(VoiceSequencer::Stop); // the rest appeared at once: no babble for it
            m_revealing = false;
            return;
        }

        // The letters revealed since last frame; at most one syllable per
        // frame, so a long frame never fires a burst.
        const std::string visible = runner.GetVisibleText();
        bool say = false;
        std::uint32_t letter = 0;
        for (; m_spoken < visible.size(); ++m_spoken)
        {
            const unsigned char c = static_cast<unsigned char>(visible[m_spoken]);
            if (!Voiced(c))
            {
                continue;
            }
            if (m_voiced++ % m_lettersPerSyllable == 0)
            {
                say = true;
                letter = c;
            }
        }
        if (say)
        {
            Send(VoiceSequencer::Say, static_cast<int>(letter), static_cast<float>(m_voiced));
        }
        if (runner.GetState() != DialogueRunner::State::Revealing)
        {
            m_revealing = false; // the line ended on its own; the last syllable decays
        }
    }
}
