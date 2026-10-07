#pragma once

#include "Core/FrameStatsWindow.h"

#include <cstdint>
#include <iosfwd>
#include <string>
#include <vector>

struct SDL_GPUDevice;
struct SDL_GPUFence;

namespace Atom
{
    // Where a click's latency goes inside the engine (M73), one sample per
    // click: from the input (the OS's timestamp, carried by SDL's event) to
    //   - the start of the frame that reads it (input waiting for a frame),
    //   - that frame's swapchain wait (the queue being full),
    //   - its submit (CPU work done),
    //   - its GPU completion (seen by polling the frame's fence twice a
    //     frame, so an upper bound within one frame).
    // PresentMon (Tools/Perf/latency.ps1) measures the whole way to the
    // display; this splits the engine's part. Their numbers aren't
    // subtracted from each other: different clocks, different inputs.
    // Only with ATOM_LATENCY_LOG=1: otherwise nothing is fenced or timed.
    class LatencyProbe
    {
    public:
        static constexpr std::size_t BlockSamples = 20;

        void SetEnabled(bool enabled, std::ostream* out) { m_enabled = enabled; m_out = out; }
        bool IsEnabled() const { return m_enabled; }

        // The frame begins (before its events are read), and the click it
        // read, if any (0 = none), both in SDL_GetTicksNS nanoseconds.
        void BeginFrame(std::uint64_t frameStartNs, std::uint64_t clickNs);
        // This frame carries a click: the renderer fences its submit.
        bool WantsFence() const { return m_enabled && m_clickNs != 0; }
        void SetWait(std::uint64_t waitNs) { m_waitNs = waitNs; }
        // The frame was submitted with this fence (owned until it signals).
        void Submitted(std::uint64_t submitNs, SDL_GPUFence* fence);
        // Resolves signalled fences into samples; prints a block when full.
        void Poll(SDL_GPUDevice* device, std::uint64_t nowNs);
        // Before the device goes: releases every fence still pending.
        void ReleaseAll(SDL_GPUDevice* device);
        // M82: the latest LAT line (empty until a block is full), for F1.
        const std::string& LastLine() const { return m_lastLine; }

        // Pure (unit-tested): one LAT line - medians and p95s in ms.
        static std::string FormatLine(int block, const FrameStatsWindow& toFrame, const FrameStatsWindow& wait,
                                      const FrameStatsWindow& toSubmit, const FrameStatsWindow& toGpu);

    private:
        struct Pending
        {
            std::uint64_t clickNs = 0;
            std::uint64_t frameStartNs = 0;
            std::uint64_t waitNs = 0;
            std::uint64_t submitNs = 0;
            SDL_GPUFence* fence = nullptr;
        };

        bool m_enabled = false;
        std::ostream* m_out = nullptr;
        std::uint64_t m_frameStartNs = 0;
        std::uint64_t m_clickNs = 0;
        std::uint64_t m_waitNs = 0;
        std::vector<Pending> m_pending;
        FrameStatsWindow m_toFrame;
        FrameStatsWindow m_wait;
        FrameStatsWindow m_toSubmit;
        FrameStatsWindow m_toGpu;
        int m_block = 0;
        std::string m_lastLine;
    };
}
