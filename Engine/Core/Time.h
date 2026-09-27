#pragma once

#include <cstdint>

namespace Atom
{
    class Time
    {
    public:
        void Reset();

        // Advances the clock and returns the clamped frame delta in seconds.
        float Tick();

        float GetDeltaSeconds() const
        {
            return m_deltaSeconds;
        }

        double GetElapsedSeconds() const
        {
            return m_elapsedSeconds;
        }

    private:
        static constexpr float MaxDeltaSeconds = 0.1f;

        std::uint64_t m_lastCounter = 0;
        float m_deltaSeconds = 0.0f;
        double m_elapsedSeconds = 0.0;
    };
}
