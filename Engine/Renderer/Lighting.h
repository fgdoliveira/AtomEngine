#pragma once

#include "Renderer/SpotLight.h"

#include <glm/vec3.hpp>

namespace Atom
{
    class Texture;

    // A light computed per pixel at runtime (M25): a point light with a
    // linear-squared falloff to zero at `radius`. Everything static is
    // baked; the few live ones are what moves or flickers (a passing train,
    // a failing sign). Submitted per frame, at most MaxLiveLights.
    struct LiveLight
    {
        glm::vec3 position{ 0.0f };
        float radius = 10.0f;
        glm::vec3 color{ 1.0f }; // linear, times intensity
    };

    inline constexpr int MaxLiveLights = 4;

    // Per-frame lighting and atmosphere. Colours are linear.
    struct SceneLighting
    {
        // Direction *towards* the sun (normalised by the renderer).
        glm::vec3 sunDirection{ 0.3f, 0.8f, 0.4f };
        glm::vec3 sunColor{ 0.45f, 0.45f, 0.45f };

        // Hemispheric ambient: light from above vs bounced from the ground.
        glm::vec3 skyColor{ 0.75f, 0.77f, 0.80f };
        glm::vec3 groundColor{ 0.20f, 0.19f, 0.17f };

        // Baked light (vertex colours, M15): 0 = hemisphere ambient only,
        // 1 = ambient from the bake (skyColor x baked sky visibility and
        // bounce). Only affects meshes that carry a bake.
        float bakedLight = 1.0f;

        // Exponential height fog. The sky is drawn in fogColor, so distant
        // geometry dissolves into it.
        glm::vec3 fogColor{ 0.34f, 0.35f, 0.37f };
        float fogDensity = 0.0f;        // per metre at fogBaseHeight; 0 = off
        float fogHeightFalloff = 0.08f; // per metre; higher = thinner up high
        float fogBaseHeight = 0.0f;     // world Y where density applies

        // Sun shadow map, centred on the camera.
        bool shadowsEnabled = true;
        float shadowHalfExtent = 30.0f;   // metres covered either side
        float shadowAmbientShare = 0.35f; // how much sky light shadows block
        float shadowNormalOffset = 0.06f; // metres; fights shadow acne

        // Glow (M23): what exceeds the threshold is blurred and added back.
        float glowStrength = 0.35f;
        float glowThreshold = 1.0f;

        // Night sky (M23): an equirectangular panorama behind everything;
        // nullptr clears to the fog colour as before.
        const Texture* skyPanorama = nullptr;
        float skyIntensity = 1.0f;

        // Day sky (M47): a gradient from horizon to zenith with the sun's
        // disc, drawn when there's no panorama.
        bool skyGradient = false;
        glm::vec3 skyZenith{ 0.22f, 0.38f, 0.70f };
        glm::vec3 skyHorizon{ 0.62f, 0.70f, 0.78f };
        float sunSize = 1.5f; // degrees
        float sunGlow = 0.4f;

        // Water (M48): tints, how much sky it shows, how much it moves.
        glm::vec3 waterShallow{ 0.16f, 0.40f, 0.40f };
        glm::vec3 waterDeep{ 0.03f, 0.12f, 0.17f };
        float waterSkyReflection = 0.85f;
        float waterRipple = 1.0f;
        float waterGlint = 1.0f; // M49

        // Rain (M50): wets upward surfaces and rings the water, 0..1.
        float rain = 0.0f;
    };
}
