#pragma once

#include <glm/vec3.hpp>

namespace AtomFramework
{
    // Small, pure pieces of the night street's live effects (M25). Pure so
    // they can be unit tested and so every user of a light (its halo, its
    // live light, its sign's glow) gets the same answer for the same input.

    // A failing tube: mostly on, with short irregular drops by up to
    // `amount` (0 = steady). `seed` is where the light is: a halo, a live
    // light and a material at the same place stutter together.
    float FlickerFactor(float time, const glm::vec3& seed, float amount);

    // Something that shuttles back and forth (a train on its line): it
    // waits at the start, travels, waits at the end, travels back.
    struct MoverState
    {
        float progress = 0.0f; // 0 at the start .. 1 at the end
        bool moving = false;
    };
    MoverState EvaluateMover(float travelSeconds, float waitSeconds, float time);

    // Moves `current` toward `target` at a rate that covers 0..1 in
    // `seconds` (a linear crossfade; 0 seconds jumps).
    float StepTowards(float current, float target, float deltaSeconds, float seconds);
}
