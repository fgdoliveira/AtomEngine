#pragma once

#include "Audio/SynthStream.h"

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace Demo
{
    class DialogueRunner;

    // Mumbled speech (Banjo-Kazooie's babble, timed like Animal Crossing's):
    // as the typewriter reveals a line, every few letters play one short
    // synthesised syllable in the speaker's voice. No words, no samples -
    // the engine's Synth (DRIFT's) on a stream of the demo's own, so it
    // shares no filter, delay or gain with anything else.

    // A voice, as data (Assets/Data/voices.json).
    struct VoicePreset
    {
        std::string name;
        Atom::Waveform waveform = Atom::Waveform::Triangle;
        float pitch = 140.0f;         // Hz, the centre of the syllables
        float pitchSpread = 0.15f;    // each syllable within pitch * (1 +- spread)
        float glide = 1.0f;           // a syllable ends at pitch * glide (< 1 falls)
        float syllableSeconds = 0.08f;
        float volume = 0.12f;
        int lettersPerSyllable = 2;   // one syllable every N voiced letters
        float breath = 0.0f;          // a noise consonant, relative to volume (0..1)
        float breathCutoff = 3000.0f; // its high-pass, Hz
        float cutoff = 2400.0f;       // the voice's low-pass, Hz
        float echo = 0.0f;            // the delay's wet level (0..1)
    };

    // One syllable, decided before it is played.
    struct Syllable
    {
        float frequency = 0.0f;
        float sweepTo = 0.0f;
        float duration = 0.0f;
        float volume = 0.0f;
        float breathVolume = 0.0f;
    };

    // Letters and digits speak; spaces and punctuation are silent. A UTF-8
    // sequence counts once (its lead byte), so an accented letter speaks.
    bool Voiced(unsigned char c);

    // Pure and deterministic: the same letter at the same place in a line
    // always sounds the same, so a line replays identically. Every value
    // stays inside the preset's bounds.
    Syllable MakeSyllable(const VoicePreset& preset, std::uint32_t letter, int index);

    // Parses voices.json: { "voices": { "<name>": { ...fields... } } }.
    // Returns the error with its JSON path, or "" on success.
    std::string ParseVoices(std::string_view json, std::vector<VoicePreset>& voices);

    // Runs on the audio thread: turns posted syllables into Synth notes.
    // Presets are set before the stream starts; selecting one is an index.
    class VoiceSequencer : public Atom::SynthSequencer
    {
    public:
        static constexpr int MaxVoices = 8;
        enum Command : int { Select = 1, Say, Stop };

        // Game thread, with the stream stopped. Extra presets are ignored.
        void SetPresets(const std::vector<VoicePreset>& presets);
        int Find(std::string_view name) const; // -1 if unknown
        const VoicePreset* Preset(int index) const
        {
            return index >= 0 && index < m_count ? &m_presets[static_cast<std::size_t>(index)] : nullptr;
        }

        void OnCommand(Atom::Synth& synth, const Atom::SynthCommand& command) override;
        void Advance(Atom::Synth& synth, double until) override;

        // Audio thread only (tests read them offline).
        int Played() const { return m_played; }
        int TooSoon() const { return m_tooSoon; }

    private:
        std::array<VoicePreset, MaxVoices> m_presets{};
        int m_count = 0;
        int m_current = 0;
        double m_lastSay = -1.0e9;
        int m_played = 0;
        int m_tooSoon = 0;
    };

    // Game thread: watches the dialogue each frame and speaks what the
    // typewriter has just revealed. Skipping (the whole line at once), a
    // new line and closing all cut the voice. Without a device it still
    // counts, and the text never waits for the sound.
    class DialogueSpeech
    {
    public:
        // Starts the stream; false if there's no audio (then silent).
        bool Start(const std::vector<VoicePreset>& presets);
        void Stop();
        // The speaker's voice for the next lines (unknown: the first).
        void SetVoice(std::string_view name);
        void SetGain(float gain);
        void Update(const DialogueRunner& runner);

        int Sent() const { return m_sent; }
        int Dropped() const { return m_dropped; }

        // For tests: the stream itself (RenderOffline).
        Atom::SynthStream& Stream() { return m_stream; }
        VoiceSequencer& Sequencer() { return m_sequencer; }

    private:
        void Send(int type, int argument = 0, float value = 0.0f);

        VoiceSequencer m_sequencer;
        Atom::SynthStream m_stream;
        bool m_running = false;
        float m_gain = -1.0f;

        int m_line = -1;              // the runner's LineSerial being spoken
        std::size_t m_spoken = 0;     // bytes of it already looked at
        int m_voiced = 0;             // voiced letters so far in the line
        int m_lettersPerSyllable = 2; // the current voice's
        bool m_revealing = false;
        int m_sent = 0;
        int m_dropped = 0;
    };
}
