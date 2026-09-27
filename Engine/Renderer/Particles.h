#pragma once

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

namespace Atom
{
    // One camera-facing, alpha-blended billboard. The layout is uploaded
    // as-is as per-instance vertex data (see Shaders/Particle.vert.hlsl).
    struct Particle
    {
        glm::vec3 position{ 0.0f };
        float size = 1.0f;              // world-space width/height
        glm::vec4 color{ 1.0f };        // linear rgb, alpha
        float rotation = 0.0f;          // radians, in screen plane
        float atlasCell = 0.0f;         // column in the particle atlas
        float padding[2]{};
    };

    static_assert(sizeof(Particle) == 48, "Particle must match the GPU layout");
}
