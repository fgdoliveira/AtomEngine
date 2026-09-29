#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <vector>

namespace Atom
{
    // A small room reverb on the master mix (M27), after Schroeder: four
    // damped feedback delays (combs) in parallel per channel, then an
    // allpass to smear their echoes into a wash. Left and right use
    // slightly different delay lengths, so the tail is wide. `size` scales
    // the delays (a hall vs a cupboard), `feedback` sets how long the tail
    // rings, `mix` how much of it is heard. Pure: no device, testable.
    class Reverb
    {
    public:
        void Configure(float mix, float size, float feedback, int sampleRate)
        {
            m_mix = std::clamp(mix, 0.0f, 1.0f);
            m_feedback = std::clamp(feedback, 0.0f, 0.95f);
            if (m_mix <= 0.0f)
            {
                return;
            }
            // Mutually prime-ish lengths (ms at size 1) avoid stacked echoes.
            static constexpr float CombMs[4] = { 29.7f, 37.1f, 41.1f, 43.7f };
            static constexpr float AllpassMs = 5.0f;
            for (int channel = 0; channel < 2; ++channel)
            {
                const float spread = channel == 0 ? 1.0f : 1.023f;
                for (int i = 0; i < 4; ++i)
                {
                    m_combs[channel][i].Resize(Samples(CombMs[i] * size * spread, sampleRate));
                }
                m_allpass[channel].Resize(Samples(AllpassMs * spread, sampleRate));
            }
        }

        bool IsActive() const { return m_mix > 0.0f; }

        void Process(float& left, float& right)
        {
            if (m_mix <= 0.0f)
            {
                return;
            }
            const float input = (left + right) * 0.5f;
            float* channels[2] = { &left, &right };
            for (int channel = 0; channel < 2; ++channel)
            {
                float wet = 0.0f;
                for (Line& comb : m_combs[channel])
                {
                    const float delayed = comb.Read();
                    // Damping: highs die first, as in a real room.
                    comb.filter += (delayed - comb.filter) * 0.4f;
                    comb.Write(input + comb.filter * m_feedback);
                    wet += delayed;
                }
                wet *= 0.25f;
                Line& allpass = m_allpass[channel];
                const float delayed = allpass.Read();
                const float out = delayed - 0.5f * wet;
                allpass.Write(wet + 0.5f * out);
                *channels[channel] += out * m_mix;
            }
        }

    private:
        struct Line
        {
            std::vector<float> buffer;
            std::size_t cursor = 0;
            float filter = 0.0f;

            void Resize(std::size_t length)
            {
                buffer.assign(std::max<std::size_t>(length, 1), 0.0f);
                cursor = 0;
                filter = 0.0f;
            }
            float Read() const { return buffer[cursor]; }
            void Write(float value)
            {
                buffer[cursor] = value;
                cursor = (cursor + 1) % buffer.size();
            }
        };

        static std::size_t Samples(float milliseconds, int sampleRate)
        {
            return static_cast<std::size_t>(milliseconds * 0.001f * static_cast<float>(sampleRate));
        }

        std::array<std::array<Line, 4>, 2> m_combs;
        std::array<Line, 2> m_allpass;
        float m_mix = 0.0f;
        float m_feedback = 0.0f;
    };
}
