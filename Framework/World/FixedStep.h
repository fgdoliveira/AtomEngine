#pragma once

#include <algorithm>

namespace AtomFramework
{
    // A fixed-timestep clock (M27): frame time accumulates and is spent in
    // whole steps, so a simulation advances identically at 30 fps or 144.
    // After a long hitch at most `maxSteps` run and the rest is dropped (the
    // "spiral of death" guard): the simulation slows instead of stalling.
    struct FixedStep
    {
        float step = 1.0f / 60.0f;
        int maxSteps = 5;
        float accumulator = 0.0f;

        // How many steps to run for this frame.
        int Advance(float deltaSeconds)
        {
            accumulator += std::max(deltaSeconds, 0.0f);
            int steps = 0;
            while (accumulator >= step && steps < maxSteps)
            {
                accumulator -= step;
                ++steps;
            }
            if (steps == maxSteps)
            {
                accumulator = std::min(accumulator, step); // drop the backlog
            }
            return steps;
        }

        // How far into the next step we are, 0..1 (for interpolation).
        float Alpha() const { return accumulator / step; }
    };
}
