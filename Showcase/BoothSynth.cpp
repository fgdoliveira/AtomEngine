#include "BoothSynth.h"

#include <algorithm>

namespace Showcase
{
    void BoothSynth::Start(Atom::Synth& synth)
    {
        // Once, before anything the keys send: the filter opens wide, the
        // delay starts off, a gentle master. (The first block applies its
        // commands before Advance, so this can't wait for Advance.)
        if (!m_started)
        {
            synth.SetFilterTarget(2400.0f, 0.01f);
            synth.SetDelay(0.47f, 0.0f, 0.0f);
            synth.SetMasterTarget(0.6f, 0.1f);
            m_started = true;
        }
    }

    void BoothSynth::OnCommand(Atom::Synth& synth, const Atom::SynthCommand& command)
    {
        Start(synth);
        switch (static_cast<BoothCommand>(command.type))
        {
        case BoothCommand::Note:
        {
            const float frequency = Notes[std::clamp(command.argument, 0, 3)];
            const double now = synth.Now();
            // The pluck: a saw with a quick attack, through the filter.
            Atom::SynthNote pluck;
            pluck.waveform = Atom::Waveform::Sawtooth;
            pluck.start = now;
            pluck.frequency = frequency;
            pluck.duration = 1.2f;
            pluck.volume = 0.12f;
            pluck.attack = 0.005f;
            synth.Play(pluck);
            // Under it, a soft sine an octave down, slower to fade.
            Atom::SynthNote body = pluck;
            body.waveform = Atom::Waveform::Sine;
            body.frequency = frequency * 0.5f;
            body.duration = 1.6f;
            body.volume = 0.08f;
            body.attack = 0.02f;
            synth.Play(body);
            break;
        }
        case BoothCommand::Filter:
            synth.SetFilterTarget(std::clamp(command.value, MinCutoff, MaxCutoff), 0.15f);
            break;
        case BoothCommand::Delay:
            // A dotted-eighth echo at 96 BPM when on; off, no feedback or wet.
            synth.SetDelay(0.47f, command.argument ? 0.4f : 0.0f, command.argument ? 0.35f : 0.0f);
            break;
        case BoothCommand::Mute:
            synth.SetMasterTarget(command.argument ? 0.0f : 0.6f, 0.1f); // a fade, never a click
            break;
        }
    }

    void BoothSynth::Advance(Atom::Synth& synth, double /*until*/)
    {
        Start(synth); // nothing is scheduled ahead: every sound is a key
    }
}
