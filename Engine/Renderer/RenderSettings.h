#pragma once

#include <glm/vec3.hpp>

#include <cstdint>

namespace Atom
{
    // Final-image look, applied in the post pass at output resolution.
    struct PostSettings
    {
        // Off = plain copy of the scene (no tonemap, grade, grain, vignette).
        bool enabled = true;

        float exposure = 1.0f;       // linear multiplier before tonemapping
        float saturation = 0.72f;    // 1 = unchanged, 0 = greyscale
        glm::vec3 tint{ 0.97f, 1.02f, 0.95f }; // slight grey-green cast
        float grain = 0.035f;        // 0 disables film grain
        float vignette = 0.25f;      // 0 disables the vignette
    };

    struct RenderSettings
    {
        // Scene resolution as a fraction of the window (1.0 = native).
        float renderScale = 1.0f;

        // 1, 2 or 4. Falls back to the highest supported count.
        std::uint32_t msaaSamples = 4;

        PostSettings post;
    };
}
