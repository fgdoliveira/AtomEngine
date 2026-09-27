#pragma once

#include <cstdint>

namespace Atom
{
    struct RenderSettings
    {
        // Scene resolution as a fraction of the window (1.0 = native).
        float renderScale = 1.0f;

        // 1, 2 or 4. Falls back to the highest supported count.
        std::uint32_t msaaSamples = 4;
    };
}
