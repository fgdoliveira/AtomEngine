#pragma once

namespace Atom
{
    // Where a frame's time went on the CPU (v0.0.15, M99), always measured
    // (a few clock reads per frame):
    //   update  - events, the developer tools and the app's OnUpdate;
    //   render  - the renderer recording and submitting the frame;
    //   wait    - the CPU waiting for the GPU: for a free frame in flight
    //             (WaitForPresentSlot) and for a swapchain image (inside
    //             Render). Mostly the GPU still drawing earlier frames, or
    //             the display's refresh when vsync caps the frame rate.
    // update + render is the CPU's work; wait is time the GPU (or the
    // display) set. There is no GPU timing: SDL_GPU has no timestamp
    // queries, so how long the GPU itself took isn't known, only how long
    // the CPU waited for it.
    struct FrameTiming
    {
        double updateMs = 0.0;
        double renderMs = 0.0;
        double waitMs = 0.0;

        double CpuMs() const { return updateMs + renderMs; }
        double TotalMs() const { return updateMs + renderMs + waitMs; }
    };

    // An exponential moving average of frame timings, so the numbers read
    // steadily in an overlay (pure: unit-tested).
    struct FrameTimingAverage
    {
        FrameTiming value;
        bool primed = false;

        void Add(const FrameTiming& t, double weight = 0.05)
        {
            if (!primed)
            {
                value = t;
                primed = true;
                return;
            }
            value.updateMs += (t.updateMs - value.updateMs) * weight;
            value.renderMs += (t.renderMs - value.renderMs) * weight;
            value.waitMs += (t.waitMs - value.waitMs) * weight;
        }
    };
}
