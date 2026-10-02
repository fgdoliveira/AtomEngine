#pragma once

#include "Core/FrameStatsWindow.h"

#include <vector>

namespace AtomGame
{
    // An in-process A/B benchmark (M46): what does switching one setting
    // cost? A laptop's speed drifts (heat, power, boost clocks), so two
    // separate runs can't be compared to a fraction of a millisecond. Here
    // A and B alternate in one process, close together in time, in the
    // order AB BA AB BA... so drift lands on both sides alike; each round's
    // *paired difference* (B - A) is the sample, and the result is their
    // median.
    //
    // Driven one frame at a time; it says when to switch the setting, waits
    // for it to settle (render targets rebuilt, pipelines created - that
    // transition isn't steady state), then measures. Pure: no clock, no
    // game - frame times come in, a schedule and results come out.
    class PairedBench
    {
    public:
        enum class Action
        {
            None,
            SetA,  // switch the setting to its A value now
            SetB,
            Done,
        };

        PairedBench(int rounds, double measureSeconds, double settleSeconds);

        // Feed the last frame's real duration.
        Action Advance(double frameMs);

        struct Round
        {
            double a = 0.0; // median frame, ms
            double b = 0.0;
            bool aFirst = true;
            double Delta() const { return b - a; }
        };
        const std::vector<Round>& GetRounds() const { return m_rounds; }

        // Over the finished rounds: the median paired difference (B - A),
        // and its smallest and largest.
        double MedianDelta() const;
        double MinDelta() const;
        double MaxDelta() const;

    private:
        enum class Phase
        {
            Switch,
            Settle,
            Measure,
        };

        bool SideIsA() const; // the half being run
        int m_roundCount;
        double m_measureSeconds;
        double m_settleSeconds;
        int m_round = 0;
        int m_half = 0;
        Phase m_phase = Phase::Switch;
        double m_elapsed = 0.0;
        Atom::FrameStatsWindow m_window;
        Round m_current;
        std::vector<Round> m_rounds;
    };
}
