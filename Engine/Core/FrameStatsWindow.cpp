#include "Core/FrameStatsWindow.h"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace Atom
{
    double FrameStatsWindow::Percentile(double p) const
    {
        if (m_samples.empty())
        {
            return 0.0;
        }
        // Sorted on demand: a window is read far less often than filled.
        std::vector<double> sorted = m_samples;
        std::sort(sorted.begin(), sorted.end());
        const double position = std::clamp(p, 0.0, 1.0) * static_cast<double>(sorted.size() - 1);
        const auto below = static_cast<std::size_t>(std::floor(position));
        const std::size_t above = std::min(below + 1, sorted.size() - 1);
        const double t = position - static_cast<double>(below);
        return sorted[below] + (sorted[above] - sorted[below]) * t;
    }

    double FrameStatsWindow::Mean() const
    {
        if (m_samples.empty())
        {
            return 0.0;
        }
        return std::accumulate(m_samples.begin(), m_samples.end(), 0.0) / static_cast<double>(m_samples.size());
    }
}
