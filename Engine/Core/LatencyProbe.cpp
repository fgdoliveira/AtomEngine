#include "Core/LatencyProbe.h"

#include <SDL3/SDL_gpu.h>

#include <cstdio>
#include <ostream>

namespace Atom
{
    namespace
    {
        double Ms(std::uint64_t ns) { return static_cast<double>(ns) / 1.0e6; }
    }

    void LatencyProbe::BeginFrame(std::uint64_t frameStartNs, std::uint64_t clickNs)
    {
        m_frameStartNs = frameStartNs;
        m_clickNs = m_enabled ? clickNs : 0;
        m_waitNs = 0;
    }

    void LatencyProbe::Submitted(std::uint64_t submitNs, SDL_GPUFence* fence)
    {
        if (!fence)
        {
            return;
        }
        m_pending.push_back({ m_clickNs, m_frameStartNs, m_waitNs, submitNs, fence });
        m_clickNs = 0;
    }

    void LatencyProbe::Poll(SDL_GPUDevice* device, std::uint64_t nowNs)
    {
        if (!m_enabled || !device)
        {
            return;
        }
        for (std::size_t i = 0; i < m_pending.size();)
        {
            Pending& pending = m_pending[i];
            if (!SDL_QueryGPUFence(device, pending.fence))
            {
                ++i;
                continue;
            }
            SDL_ReleaseGPUFence(device, pending.fence);
            // A click stamped after its frame began can't happen; clamp so
            // a clock quirk can't make a negative sample.
            const std::uint64_t click = pending.clickNs <= pending.frameStartNs ? pending.clickNs : pending.frameStartNs;
            m_toFrame.AddSample(Ms(pending.frameStartNs - click));
            m_wait.AddSample(Ms(pending.waitNs));
            m_toSubmit.AddSample(Ms(pending.submitNs - click));
            m_toGpu.AddSample(Ms(nowNs - click));
            m_pending.erase(m_pending.begin() + static_cast<std::ptrdiff_t>(i));

            if (m_toGpu.Count() >= BlockSamples)
            {
                if (m_out)
                {
                    *m_out << FormatLine(m_block, m_toFrame, m_wait, m_toSubmit, m_toGpu) << std::endl;
                }
                ++m_block;
                m_toFrame.Clear();
                m_wait.Clear();
                m_toSubmit.Clear();
                m_toGpu.Clear();
            }
        }
    }

    void LatencyProbe::ReleaseAll(SDL_GPUDevice* device)
    {
        if (device)
        {
            for (const Pending& pending : m_pending)
            {
                SDL_ReleaseGPUFence(device, pending.fence);
            }
        }
        m_pending.clear();
    }

    std::string LatencyProbe::FormatLine(int block, const FrameStatsWindow& toFrame, const FrameStatsWindow& wait,
                                         const FrameStatsWindow& toSubmit, const FrameStatsWindow& toGpu)
    {
        char line[320];
        std::snprintf(line, sizeof(line),
                      "LAT block %d samples %zu input_to_frame %.2f p95 %.2f wait %.2f p95 %.2f "
                      "input_to_submit %.2f p95 %.2f input_to_gpu %.2f p95 %.2f",
                      block, toGpu.Count(), toFrame.Median(), toFrame.Percentile(0.95), wait.Median(),
                      wait.Percentile(0.95), toSubmit.Median(), toSubmit.Percentile(0.95), toGpu.Median(),
                      toGpu.Percentile(0.95));
        return line;
    }
}
