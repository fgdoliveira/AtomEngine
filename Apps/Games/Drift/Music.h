#pragma once

// DRIFT's generative soundtrack (M80), the three.js original's Music
// (src/audio.js) ported to the engine's Synth: 96 BPM eighth-note steps;
// four chords on D minor as pads (two detuned saws); a kick every quarter;
// hats once flow passes 0.15; an arpeggio more likely with flow; the
// filtered bus's low-pass opening as 500 + flow^2 * 6000 Hz; pickups that
// climb the pentatonic with the orb chain, a chime for a gate, a thud for
// a rock. Runs on the audio thread (SynthStream); the game posts commands.

#include "Audio/SynthStream.h"

#include <cstdint>
#include <random>

namespace Drift
{
    enum class MusicCommand : int { Flow = 1, Pickup = 2, Chime = 3, Thud = 4, ToggleMute = 5 };

    class Music : public Atom::SynthSequencer
    {
    public:
        static constexpr float Bpm = 96.0f;
        static constexpr double Beat = 60.0 / Bpm;
        static constexpr int Root = 50; // D3

        explicit Music(std::uint32_t seed = 1);

        void OnCommand(Atom::Synth& synth, const Atom::SynthCommand& command) override;
        void Advance(Atom::Synth& synth, double until) override;

        static float MidiToFrequency(float midi);

        // For tests: the step counter and the time of the next step.
        int Step() const { return m_step; }
        double NextStepTime() const { return m_next; }
        float Flow() const { return m_flow; }

    private:
        void Schedule(Atom::Synth& synth, double time, int step);
        void Pad(Atom::Synth& synth, int midi, double time, float duration);
        void Pluck(Atom::Synth& synth, int midi, double time, float volume);
        void Kick(Atom::Synth& synth, double time, float volume);
        void Hat(Atom::Synth& synth, double time);
        void Pickup(Atom::Synth& synth, int chain);
        void Chime(Atom::Synth& synth);
        void Thud(Atom::Synth& synth);

        bool m_started = false;
        double m_next = 0.1; // the first step, 0.1 s in (as the original)
        int m_step = 0;
        float m_flow = 0.0f;
        std::mt19937 m_random;
        std::uniform_real_distribution<float> m_unit{ 0.0f, 1.0f };
    };
}
