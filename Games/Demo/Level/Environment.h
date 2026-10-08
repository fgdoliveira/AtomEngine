#pragma once

#include <glm/vec3.hpp>

#include <optional>

namespace Demo
{
    // A procedural day sky (M47): zenith colour overhead, horizon colour at
    // eye level, the sun's disc and a glow around it. Drawn where a level
    // has no panorama; the water reflects the same function (M48).
    struct SkyGradient
    {
        glm::vec3 zenith{ 0.22f, 0.38f, 0.70f };
        glm::vec3 horizon{ 0.62f, 0.70f, 0.78f };
        float sunSize = 1.5f; // degrees: the disc's radius
        float sunGlow = 0.4f; // how much the air around the sun lights up
    };

    // Stylized water (M48): authored colours, not optics. Shallow water
    // shows its tint, deep water its depth colour; at grazing angles the
    // sky takes over (Fresnel), by up to skyReflection.
    struct WaterLook
    {
        glm::vec3 shallow{ 0.16f, 0.40f, 0.40f };
        glm::vec3 deep{ 0.03f, 0.12f, 0.17f };
        float skyReflection = 0.85f; // 0..1
        float ripple = 1.0f;         // how strongly the surface moves
        float glint = 1.0f;          // M49: the sun's highlight (low under cloud)
        bool reflection = false;     // M51: mirror the near scene in it (a render pass)
    };

    // The part of a level's light that weather and time of day change
    // (M47): plain numbers, so two states can be blended. A level's
    // lighting is one; presets (M49) resolve into one.
    struct EnvironmentState
    {
        glm::vec3 sunDirection{ 0.35f, 0.6f, -0.55f };
        glm::vec3 sunColor{ 0.45f };
        glm::vec3 skyColor{ 0.75f, 0.77f, 0.80f };   // ambient from above
        glm::vec3 groundColor{ 0.20f, 0.19f, 0.17f }; // ambient bounced from below
        glm::vec3 fogColor{ 0.46f, 0.47f, 0.47f };
        std::optional<float> fogDensity; // unset: the player's fog setting (F5)
        std::optional<SkyGradient> sky;  // unset: no day sky (clear to fog colour)
        WaterLook water;                 // M48: how any water in the level looks
        float rain = 0.0f;               // M50: 0 dry .. 1 downpour (outdoors only)
        glm::vec3 wind{ 0.45f, 0.0f, 0.15f }; // M50: m/s; sway, drifting leaves, slanting rain
    };

    // a at t = 0, b at t = 1. Colours and numbers mix linearly, the sun's
    // direction mixes and renormalises, and what can't mix (one side has no
    // sky, or no fog density) switches at the midpoint.
    EnvironmentState Blend(const EnvironmentState& a, const EnvironmentState& b, float t);
}
