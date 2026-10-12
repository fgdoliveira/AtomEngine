#include "Music.h"

#include <array>
#include <cmath>

namespace Drift
{
    namespace
    {
        // D minor pentatonic, for the arpeggio and the pickups.
        constexpr std::array<int, 5> Scale{ 0, 3, 5, 7, 10 };
        constexpr std::array<std::array<int, 5>, 4> Chords{ {
            { 0, 3, 7, 10, 14 }, { -4, 0, 3, 7, 10 }, { -7, -3, 0, 5, 9 }, { -2, 2, 5, 9, 12 } } };
    }

    Music::Music(std::uint32_t seed) : m_random(seed)
    {
    }

    float Music::MidiToFrequency(float midi)
    {
        return 440.0f * std::pow(2.0f, (midi - 69.0f) / 12.0f);
    }

    void Music::OnCommand(Atom::Synth& synth, const Atom::SynthCommand& command)
    {
        switch (static_cast<MusicCommand>(command.type))
        {
        case MusicCommand::Flow:
            m_flow = command.value;
            // filter.frequency.setTargetAtTime(500 + f*f*6000, now, 0.3)
            synth.SetFilterTarget(500.0f + m_flow * m_flow * 6000.0f, 0.3f);
            break;
        case MusicCommand::Pickup:
            Pickup(synth, command.argument);
            break;
        case MusicCommand::Chime:
            Chime(synth);
            break;
        case MusicCommand::Thud:
            Thud(synth);
            break;
        case MusicCommand::ToggleMute:
            synth.SetMasterTarget(synth.MasterTarget() > 0.1f ? 0.0f : 0.7f, 0.1f);
            break;
        }
    }

    void Music::Advance(Atom::Synth& synth, double until)
    {
        if (!m_started)
        {
            // The original's graph: filter at 600 Hz, Q 2 dB; a delay of
            // 0.75 beat, feedback 0.35, wet 0.3; master 0.7.
            synth.SetFilterQ(std::pow(10.0f, 2.0f / 20.0f));
            synth.SetDelay(static_cast<float>(Beat * 0.75), 0.35f, 0.3f);
            synth.SetMasterTarget(0.7f, 0.1f);
            m_next = synth.Now() + 0.1;
            m_started = true;
        }
        // The original schedules 0.12 s ahead from a 25 ms timer; on the
        // audio thread each step is scheduled just before its own block.
        while (m_next < until)
        {
            Schedule(synth, m_next, m_step);
            m_next += Beat / 2.0;
            ++m_step;
        }
    }

    void Music::Schedule(Atom::Synth& synth, double time, int step) // step counts eighth notes
    {
        const int bar = step / 8;
        const std::array<int, 5>& chord = Chords[bar % Chords.size()];
        if (step % 8 == 0)
        {
            for (int note : chord)
            {
                Pad(synth, Root + note, time, static_cast<float>(Beat * 4.0));
            }
        }
        if (step % 2 == 0)
        {
            Kick(synth, time, step % 8 == 0 ? 1.0f : 0.6f);
        }
        if (m_flow > 0.15f && step % 2 == 1)
        {
            Hat(synth, time);
        }
        // The arpeggio walks the chord, more notes with more flow.
        if (m_unit(m_random) < 0.45f + m_flow * 0.5f)
        {
            const int note = chord[(step * 3) % chord.size()] + 12 + (step % 4 == 3 ? 12 : 0);
            Pluck(synth, Root + note, time, 0.07f);
        }
    }

    void Music::Pad(Atom::Synth& synth, int midi, double time, float duration)
    {
        for (float detune : { -7.0f, 7.0f })
        {
            Atom::SynthNote n;
            n.waveform = Atom::Waveform::Sawtooth;
            n.start = time;
            n.frequency = MidiToFrequency(static_cast<float>(midi));
            n.detuneCents = detune;
            n.duration = duration;
            n.volume = 0.022f;
            n.attack = 0.8f;
            n.bus = Atom::SynthBus::Filtered;
            synth.Play(n);
        }
    }

    void Music::Pluck(Atom::Synth& synth, int midi, double time, float volume)
    {
        Atom::SynthNote n;
        n.waveform = Atom::Waveform::Triangle;
        n.start = time;
        n.frequency = MidiToFrequency(static_cast<float>(midi));
        n.duration = 0.35f;
        n.volume = volume;
        n.bus = Atom::SynthBus::Filtered;
        synth.Play(n);
    }

    void Music::Kick(Atom::Synth& synth, double time, float volume)
    {
        // A sine from 120 to 40 Hz over 0.15 s; gain 0.5*vol to 0.0001 over 0.3 s.
        Atom::SynthNote n;
        n.waveform = Atom::Waveform::Sine;
        n.start = time;
        n.frequency = 120.0f;
        n.sweepTo = 40.0f;
        n.sweepTime = 0.15f;
        n.duration = 0.3f;
        n.volume = 0.5f * volume;
        n.attack = 0.0f;
        n.bus = Atom::SynthBus::Master;
        synth.Play(n);
    }

    void Music::Hat(Atom::Synth& synth, double time)
    {
        Atom::SynthNote n;
        n.waveform = Atom::Waveform::Noise;
        n.start = time;
        n.duration = 0.05f;
        n.volume = 0.05f * m_flow;
        n.attack = 0.0f;
        n.noiseFilter = Atom::NoiseFilter::Highpass;
        n.noiseCutoff = 7000.0f;
        n.bus = Atom::SynthBus::Master;
        synth.Play(n);
    }

    void Music::Pickup(Atom::Synth& synth, int chain)
    {
        // The chain climbs the pentatonic, so orb runs become melodies.
        const int degree = Scale[chain % Scale.size()] + 12 * ((chain / static_cast<int>(Scale.size())) % 3);
        const double now = synth.Now();
        Atom::SynthNote a;
        a.waveform = Atom::Waveform::Sine;
        a.start = now;
        a.frequency = MidiToFrequency(static_cast<float>(Root + 24 + degree));
        a.duration = 0.5f;
        a.volume = 0.12f;
        a.bus = Atom::SynthBus::Effects;
        synth.Play(a);
        Atom::SynthNote b = a;
        b.waveform = Atom::Waveform::Triangle;
        b.frequency = MidiToFrequency(static_cast<float>(Root + 36 + degree));
        b.duration = 0.2f;
        b.volume = 0.04f;
        synth.Play(b);
    }

    void Music::Chime(Atom::Synth& synth)
    {
        const double now = synth.Now();
        const std::array<int, 3> notes{ 0, 7, 12 };
        for (std::size_t k = 0; k < notes.size(); ++k)
        {
            Atom::SynthNote n;
            n.waveform = Atom::Waveform::Sine;
            n.start = now + static_cast<double>(k) * 0.06;
            n.frequency = MidiToFrequency(static_cast<float>(Root + 36 + notes[k]));
            n.duration = 0.9f;
            n.volume = 0.08f;
            n.bus = Atom::SynthBus::Effects;
            synth.Play(n);
        }
    }

    void Music::Thud(Atom::Synth& synth)
    {
        const double now = synth.Now();
        Atom::SynthNote noise;
        noise.waveform = Atom::Waveform::Noise;
        noise.start = now;
        noise.duration = 0.4f;
        noise.volume = 0.4f;
        noise.attack = 0.0f;
        noise.noiseFilter = Atom::NoiseFilter::Lowpass;
        noise.noiseCutoff = 300.0f;
        noise.bus = Atom::SynthBus::Master;
        synth.Play(noise);
        Atom::SynthNote low;
        low.waveform = Atom::Waveform::Sine;
        low.start = now;
        low.frequency = 55.0f;
        low.duration = 0.4f;
        low.volume = 0.3f;
        low.bus = Atom::SynthBus::Master;
        synth.Play(low);
    }
}
