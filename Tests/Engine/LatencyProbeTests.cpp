#include "Core/LatencyProbe.h"

#include <doctest/doctest.h>

using namespace Atom;

TEST_CASE("LAT line: the exact format latency tooling reads")
{
    FrameStatsWindow toFrame, wait, toSubmit, toGpu;
    for (double ms : { 1.0, 2.0, 3.0 })
    {
        toFrame.AddSample(ms);
        wait.AddSample(ms * 2.0);
        toSubmit.AddSample(ms * 3.0);
        toGpu.AddSample(ms * 4.0);
    }
    CHECK(LatencyProbe::FormatLine(7, toFrame, wait, toSubmit, toGpu)
          == "LAT block 7 samples 3 input_to_frame 2.00 p95 2.90 wait 4.00 p95 5.80 "
             "input_to_submit 6.00 p95 8.70 input_to_gpu 8.00 p95 11.60");
}

TEST_CASE("LatencyProbe: off, a click never asks for a fence")
{
    LatencyProbe probe;
    probe.BeginFrame(1'000'000, 900'000);
    CHECK_FALSE(probe.WantsFence());
}

TEST_CASE("LatencyProbe: on, only a frame that read a click is fenced")
{
    LatencyProbe probe;
    probe.SetEnabled(true, nullptr);
    probe.BeginFrame(1'000'000, 0);
    CHECK_FALSE(probe.WantsFence());
    probe.BeginFrame(2'000'000, 1'500'000);
    CHECK(probe.WantsFence());
    probe.Submitted(2'100'000, nullptr); // no fence (failed submit): nothing pending
    CHECK(probe.WantsFence());           // the click still waits for a fenced submit
}
