#pragma once

#include <cstddef>
#include <vector>

namespace Atom
{
    // A window of frame times (M46) and their summary: the median (the
    // typical frame), a percentile such as the 95th (how bad the slow ones
    // get) and the mean (pulled by spikes - a diagnostic). Nothing else:
    // no clock, no logging, no rendering. Benchmarks fill it and read it.
    class FrameStatsWindow
    {
    public:
        void AddSample(double milliseconds) { m_samples.push_back(milliseconds); }
        void Clear() { m_samples.clear(); }
        std::size_t Count() const { return m_samples.size(); }

        // 0 when empty.
        double Median() const { return Percentile(0.5); }
        // p in [0, 1]: the value below which that share of the samples
        // lies, interpolated between the two nearest (0.95 = "p95").
        double Percentile(double p) const;
        double Mean() const;

    private:
        std::vector<double> m_samples;
    };
}
