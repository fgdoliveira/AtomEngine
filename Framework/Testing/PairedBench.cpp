#include "Testing/PairedBench.h"

#include <algorithm>

namespace AtomFramework
{
    PairedBench::PairedBench(int rounds, double measureSeconds, double settleSeconds)
        : m_roundCount(std::max(1, rounds))
        , m_measureSeconds(std::max(0.05, measureSeconds))
        , m_settleSeconds(std::max(0.0, settleSeconds))
    {
    }

    bool PairedBench::SideIsA() const
    {
        // AB BA AB BA: even rounds start with A, odd ones with B.
        const bool aFirst = m_round % 2 == 0;
        return (m_half == 0) == aFirst;
    }

    PairedBench::Action PairedBench::Advance(double frameMs)
    {
        if (m_round >= m_roundCount)
        {
            return Action::Done;
        }
        switch (m_phase)
        {
        case Phase::Switch:
            // The frame this returns is still the old setting's: settle.
            m_phase = Phase::Settle;
            m_elapsed = 0.0;
            return SideIsA() ? Action::SetA : Action::SetB;
        case Phase::Settle:
            m_elapsed += frameMs / 1000.0;
            if (m_elapsed >= m_settleSeconds)
            {
                m_phase = Phase::Measure;
                m_elapsed = 0.0;
                m_window.Clear();
            }
            return Action::None;
        case Phase::Measure:
            m_window.AddSample(frameMs);
            m_elapsed += frameMs / 1000.0;
            if (m_elapsed < m_measureSeconds)
            {
                return Action::None;
            }
            (SideIsA() ? m_current.a : m_current.b) = m_window.Median();
            if (m_half == 0)
            {
                m_current.aFirst = SideIsA();
                m_half = 1;
            }
            else
            {
                m_rounds.push_back(m_current);
                m_current = Round{};
                m_half = 0;
                ++m_round;
                if (m_round >= m_roundCount)
                {
                    return Action::Done;
                }
            }
            m_phase = Phase::Switch;
            return Action::None;
        }
        return Action::None;
    }

    double PairedBench::MedianDelta() const
    {
        Atom::FrameStatsWindow deltas;
        for (const Round& round : m_rounds)
        {
            deltas.AddSample(round.Delta());
        }
        return deltas.Median();
    }

    double PairedBench::MinDelta() const
    {
        double low = m_rounds.empty() ? 0.0 : m_rounds.front().Delta();
        for (const Round& round : m_rounds)
        {
            low = std::min(low, round.Delta());
        }
        return low;
    }

    double PairedBench::MaxDelta() const
    {
        double high = m_rounds.empty() ? 0.0 : m_rounds.front().Delta();
        for (const Round& round : m_rounds)
        {
            high = std::max(high, round.Delta());
        }
        return high;
    }
}
