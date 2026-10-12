#pragma once

#include "Audio/SynthStream.h"

namespace Showcase
{
    // What the booth's keys send to the audio thread (SynthCommand::type).
    enum class BoothCommand : int
    {
        Note = 1,   // argument: 0..3, the four keys
        Filter,     // value: the low-pass cutoff to glide to (Hz)
        Delay,      // argument: 1 on, 0 off
        Mute,       // argument: 1 silent, 0 audible (the lens's A/B)
    };

    // The synth booth (v0.0.14): Atom::Synth played live from the keys - a
    // plucked saw an octave over a soft sine, on the filtered bus, through
    // the feedback delay when it's on. Notes are A minor pentatonic, so any
    // order sounds right. Nothing is scheduled ahead: every sound is a key.
    class BoothSynth final : public Atom::SynthSequencer
    {
    public:
        static constexpr float Notes[4] = { 220.0f, 261.63f, 293.66f, 329.63f }; // A3 C4 D4 E4
        static constexpr float MinCutoff = 250.0f;
        static constexpr float MaxCutoff = 8000.0f;

        void OnCommand(Atom::Synth& synth, const Atom::SynthCommand& command) override;
        void Advance(Atom::Synth& synth, double until) override;

    private:
        void Start(Atom::Synth& synth);

        bool m_started = false;
    };
}
