#include "Audio/Synth.h"

#include <algorithm>
#include <cmath>

namespace Atom
{
    namespace
    {
        constexpr double Pi = 3.14159265358979323846;
        constexpr float Floor = 0.0001f; // WebAudio's exponential ramps end here

        // PolyBLEP: rounds the saw's jump over one sample, so it doesn't alias
        // (WebAudio's oscillators are band-limited too).
        double PolyBlep(double t, double dt)
        {
            if (t < dt)
            {
                t /= dt;
                return t + t - t * t - 1.0;
            }
            if (t > 1.0 - dt)
            {
                t = (t - 1.0) / dt;
                return t * t + t + t + 1.0;
            }
            return 0.0;
        }
    }

    void Biquad::SetLowpass(float cutoff, float q, float sampleRate)
    {
        const double w = 2.0 * Pi * std::clamp(cutoff, 10.0f, sampleRate * 0.45f) / sampleRate;
        const double alpha = std::sin(w) / (2.0 * q);
        const double c = std::cos(w);
        const double a0 = 1.0 + alpha;
        m_b0 = static_cast<float>((1.0 - c) / 2.0 / a0);
        m_b1 = static_cast<float>((1.0 - c) / a0);
        m_b2 = m_b0;
        m_a1 = static_cast<float>(-2.0 * c / a0);
        m_a2 = static_cast<float>((1.0 - alpha) / a0);
    }

    void Biquad::SetHighpass(float cutoff, float q, float sampleRate)
    {
        const double w = 2.0 * Pi * std::clamp(cutoff, 10.0f, sampleRate * 0.45f) / sampleRate;
        const double alpha = std::sin(w) / (2.0 * q);
        const double c = std::cos(w);
        const double a0 = 1.0 + alpha;
        m_b0 = static_cast<float>((1.0 + c) / 2.0 / a0);
        m_b1 = static_cast<float>(-(1.0 + c) / a0);
        m_b2 = m_b0;
        m_a1 = static_cast<float>(-2.0 * c / a0);
        m_a2 = static_cast<float>((1.0 - alpha) / a0);
    }

    float Biquad::Process(float x)
    {
        const float y = m_b0 * x + m_b1 * m_x1 + m_b2 * m_x2 - m_a1 * m_y1 - m_a2 * m_y2;
        m_x2 = m_x1;
        m_x1 = x;
        m_y2 = m_y1;
        m_y1 = y;
        return y;
    }

    Synth::Synth(float sampleRate) : m_sampleRate(sampleRate)
    {
        m_filter.SetLowpass(m_cutoff, m_filterQ, m_sampleRate);
    }

    bool Synth::Play(const SynthNote& note)
    {
        for (Voice& voice : m_voices)
        {
            if (!voice.active)
            {
                voice = Voice{};
                voice.active = true;
                voice.note = note;
                const double span = std::max(1e-4, static_cast<double>(note.duration - note.attack));
                voice.decayRatio = std::pow(static_cast<double>(Floor) / std::max(note.volume, Floor), 1.0 / (span * m_sampleRate));
                voice.frequency = note.frequency * std::pow(2.0, note.detuneCents / 1200.0);
                if (note.sweepTime > 0.0f && note.sweepTo > 0.0f)
                {
                    voice.sweepRatio = std::pow(static_cast<double>(note.sweepTo) / note.frequency, 1.0 / (note.sweepTime * m_sampleRate));
                }
                if (note.waveform == Waveform::Noise)
                {
                    // WebAudio's default biquad Q is 1 dB.
                    constexpr float q = 1.1220f;
                    if (note.noiseFilter == NoiseFilter::Lowpass)
                    {
                        voice.noise.SetLowpass(note.noiseCutoff, q, m_sampleRate);
                    }
                    else if (note.noiseFilter == NoiseFilter::Highpass)
                    {
                        voice.noise.SetHighpass(note.noiseCutoff, q, m_sampleRate);
                    }
                }
                return true;
            }
        }
        return false;
    }

    void Synth::SetFilterTarget(float cutoff, float timeConstant)
    {
        m_cutoffTarget = cutoff;
        m_cutoffTimeConstant = std::max(timeConstant, 1e-3f);
    }

    void Synth::SetDelay(float seconds, float feedback, float wet)
    {
        m_delaySamples = std::clamp(static_cast<int>(seconds * m_sampleRate), 1, static_cast<int>(m_delayLine.size()) - 1);
        m_feedback = feedback;
        m_wet = wet;
    }

    void Synth::SetMasterTarget(float gain, float timeConstant)
    {
        m_masterTarget = gain;
        m_masterTimeConstant = std::max(timeConstant, 1e-3f);
    }

    int Synth::ActiveVoices() const
    {
        return static_cast<int>(std::count_if(m_voices.begin(), m_voices.end(), [](const Voice& v) { return v.active; }));
    }

    float Synth::VoiceSample(Voice& voice, double time)
    {
        const SynthNote& n = voice.note;
        const double t = time - n.start;
        if (t < 0.0)
        {
            return 0.0f; // not yet
        }
        // Stops 50 ms after its envelope ends, as the original's o.stop().
        if (t > n.duration + 0.05)
        {
            voice.active = false;
            return 0.0f;
        }

        // Envelope: linear to the peak over `attack`, then exponentially to
        // 0.0001 at `duration` (WebAudio linear/exponentialRampToValueAtTime).
        float gain = 0.0f;
        if (n.attack > 0.0f && t < n.attack)
        {
            gain = static_cast<float>(n.volume * t / n.attack);
        }
        else if (t < n.duration)
        {
            if (!voice.decaying)
            {
                voice.decaying = true;
                voice.envelope = n.volume;
            }
            else
            {
                voice.envelope *= voice.decayRatio;
            }
            gain = static_cast<float>(voice.envelope);
        }

        if (n.waveform == Waveform::Noise)
        {
            const float white = static_cast<float>(m_noise()) / static_cast<float>(std::minstd_rand::max()) * 2.0f - 1.0f;
            const float filtered = n.noiseFilter == NoiseFilter::None ? white : voice.noise.Process(white);
            return filtered * gain;
        }

        // The kick's sweep: exponential until sweepTime, then held.
        if (voice.sweepRatio != 1.0 && t < n.sweepTime)
        {
            voice.frequency *= voice.sweepRatio;
        }
        const double dt = voice.frequency / m_sampleRate;
        double sample = 0.0;
        switch (n.waveform)
        {
        case Waveform::Sine:
            sample = std::sin(2.0 * Pi * voice.phase);
            break;
        case Waveform::Triangle:
            sample = 1.0 - 4.0 * std::abs(voice.phase - 0.5);
            break;
        case Waveform::Sawtooth:
            sample = 2.0 * voice.phase - 1.0 - PolyBlep(voice.phase, dt);
            break;
        case Waveform::Noise:
            break;
        }
        voice.phase += dt;
        voice.phase -= std::floor(voice.phase);
        return static_cast<float>(sample) * gain;
    }

    void Synth::Render(float* out, int frames)
    {
        // Per block: the filter's cutoff and the master gain approach their
        // targets (exact for a one-pole: 1 - e^(-block / tau)).
        const float block = static_cast<float>(frames) / m_sampleRate;
        m_cutoff += (m_cutoffTarget - m_cutoff) * (1.0f - std::exp(-block / m_cutoffTimeConstant));
        m_filter.SetLowpass(m_cutoff, m_filterQ, m_sampleRate);
        const float masterFrom = m_master;
        m_master += (m_masterTarget - m_master) * (1.0f - std::exp(-block / m_masterTimeConstant));

        for (int i = 0; i < frames; ++i)
        {
            const double time = static_cast<double>(m_frame + i) / m_sampleRate;
            float filtered = 0.0f;
            float effects = 0.0f;
            float direct = 0.0f;
            for (Voice& voice : m_voices)
            {
                if (!voice.active)
                {
                    continue;
                }
                const float s = VoiceSample(voice, time);
                switch (voice.note.bus)
                {
                case SynthBus::Filtered: filtered += s; break;
                case SynthBus::Effects: effects += s; break;
                case SynthBus::Master: direct += s; break;
                }
            }
            // The graph: filter -> master and delay; effects -> master and
            // delay; delay -> itself (feedback) and, wet, -> master.
            const float filterOut = m_filter.Process(filtered);
            const int read = (m_delayWrite - m_delaySamples) & (static_cast<int>(m_delayLine.size()) - 1);
            const float delayed = m_delayLine[read];
            m_delayLine[m_delayWrite] = filterOut + effects + delayed * m_feedback;
            m_delayWrite = (m_delayWrite + 1) & (static_cast<int>(m_delayLine.size()) - 1);

            const float gain = masterFrom + (m_master - masterFrom) * (static_cast<float>(i) / static_cast<float>(frames));
            out[i] = (filterOut + effects + direct + delayed * m_wet) * gain;
        }
        m_frame += static_cast<std::uint64_t>(frames);
    }
}
