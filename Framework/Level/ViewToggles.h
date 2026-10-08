#pragma once

#include "Level/Environment.h"
#include "Renderer/Renderer.h"

#include <cstddef>
#include <optional>

namespace Atom
{
    class Input;
}

namespace AtomFramework
{
    class Atmosphere;
    class Level;

    // The fog choices F5 steps through (density: ~3/density metres until
    // fully fogged). "level" keeps the level's own (M47).
    struct FogPreset
    {
        const char* name;
        float density;
    };
    inline constexpr FogPreset FogPresets[] = {
        { "dense", 0.13f },
        { "medium", 0.085f },
        { "light", 0.045f },
        { "off", 0.0f },
        { "level", -1.0f },
    };
    inline constexpr std::size_t DefaultFogPreset = 4; // level

    // What the viewer may switch on top of a level's own look (v0.0.14,
    // shared by every app since M86; the demo's since v0.0.1): the F2-F8
    // keys, and the same switches the scenarios and panels set.
    struct ViewToggles
    {
        std::size_t fogPreset = DefaultFogPreset;
        bool shadows = true;
        bool bakedLight = true;     // F3: compare with the flat ambient
        int postMode = 0;           // 0 full, 1 grade only, 2 off
        bool sun = true;
        std::optional<float> rain;  // M51: "set rain" for benchmarks
    };

    // The renderer's lighting for `level` (null: none loaded) under the
    // current environment (the level's own light, or a preset over it,
    // M49) with the viewer's toggles on top.
    Atom::SceneLighting SceneLightingFor(const Level* level, const EnvironmentState& environment,
                                         const ViewToggles& toggles);

    // F2 render scale, F3 baked light, F4 MSAA, F5 fog, F6 shadows, F7 post
    // look, F8 particles (when `particles` is given). Settings that live in
    // the renderer are applied here; returns true when the lighting changed
    // (the caller reapplies SceneLightingFor).
    bool HandleViewKeys(const Atom::Input& input, Atom::Renderer& renderer, ViewToggles& toggles,
                        Atmosphere* particles);
}
