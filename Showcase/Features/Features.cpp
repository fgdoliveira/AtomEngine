#include "Features/Features.h"

namespace Showcase
{
    const std::vector<FeatureInfo>& FeatureCatalog()
    {
        static const std::vector<FeatureInfo> features = {
            { "water", "Stylized water", "Water shader: Fresnel, ripples, depth colour, sun glints",
              "Shaders/Water.frag.hlsl", 65, "", { 0.0f, 0.3f, -26.0f }, true },
            { "reflection", "Planar reflection", "The scene drawn again from a camera mirrored in the water",
              "Engine/Renderer/Renderer.cpp", 68, "", { 0.0f, 0.3f, -21.0f }, true },
            { "shadows", "Sun shadows", "Directional shadow map, texel-snapped, PCF filtered",
              "Shaders/Shadow.vert.hlsl", 23, "", {}, false },
            { "fog", "Height fog", "Exponential height fog in the scene shader",
              "Shaders/Common.hlsli", 22, "", {}, false },
            { "weather", "Weather and wind", "Environment presets blended over the level; rain and leaves as particles",
              "Framework/Environment/Atmosphere.cpp", 67, "", {}, false },
            { "lights", "Live lights", "Per-pixel point lights, culled per draw (at most 4)",
              "Engine/Renderer/Renderer.cpp", 43, "lamp_2", {}, true },
            { "glow", "Glow and halos", "A bright pass blurred and added back; additive halo billboards",
              "Engine/Renderer/Glow.cpp", 41, "sign_pink", {}, true },
            { "toon", "Toon shading and outlines", "A shader variant with three light steps; an inverted-hull outline pass",
              "Shaders/BasicToon.frag.hlsl", 98, "toon_hokora", {}, true },
            { "character", "Skinned character", "Skinning, blended clips and an animation state machine",
              "Framework/Character/Animator.cpp", 55, "rudy", {}, true },
            { "synth", "Live synth", "Synthesised on the audio thread; keys reach it through a lock-free queue",
              "Engine/Audio/Synth.cpp", 99, "synth_booth", {}, true },
        };
        return features;
    }
}
