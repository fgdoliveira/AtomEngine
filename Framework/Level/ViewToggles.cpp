#include "Level/ViewToggles.h"

#include "Environment/Atmosphere.h"
#include "Level/Level.h"
#include "Platform/Input.h"

#include <SDL3/SDL.h>

#include "Scene/Camera.h"

#include <glm/trigonometric.hpp>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <iterator>

namespace AtomFramework
{
    Atom::SceneLighting SceneLightingFor(const Level* level, const EnvironmentState& e, const ViewToggles& toggles)
    {
        // The level decides the light; the viewer's toggles (fog preset,
        // shadows) apply on top wherever they are.
        Atom::SceneLighting lighting{};
        float levelFog = 0.0f;
        if (level)
        {
            const LevelLighting& l = level->GetData().lighting;
            // Sun, ambient, fog, sky and water from the environment; the
            // rest stays the level's.
            lighting.sunDirection = e.sunDirection;
            lighting.sunColor = toggles.sun ? e.sunColor : glm::vec3{ 0.0f };
            lighting.skyColor = e.skyColor;
            lighting.groundColor = e.groundColor;
            lighting.fogColor = e.fogColor;
            lighting.shadowsEnabled = l.shadows && toggles.shadows;
            lighting.bakedLight = toggles.bakedLight ? l.bakedLight : 0.0f;
            lighting.glowStrength = toggles.glow ? l.glowStrength : 0.0f;
            lighting.glowThreshold = l.glowThreshold;
            lighting.skyPanorama = level->GetSkyPanorama();
            lighting.skyIntensity = level->GetData().sky ? level->GetData().sky->intensity : 1.0f;
            if (e.sky)
            {
                lighting.skyGradient = true;
                lighting.skyZenith = e.sky->zenith;
                lighting.skyHorizon = e.sky->horizon;
                lighting.sunSize = e.sky->sunSize;
                lighting.sunGlow = e.sky->sunGlow;
            }
            levelFog = e.fogDensity.value_or(0.0f);
            lighting.waterShallow = e.water.shallow;
            lighting.waterDeep = e.water.deep;
            lighting.waterSkyReflection = e.water.skyReflection;
            lighting.waterRipple = e.water.ripple;
            lighting.waterGlint = e.water.glint;
            lighting.waterReflection = e.water.reflection;
            lighting.rain = level->GetData().outdoor ? toggles.rain.value_or(e.rain) : 0.0f; // indoors it rains elsewhere
        }
        const float presetFog = FogPresets[toggles.fogPreset].density;
        lighting.fogDensity = presetFog < 0.0f ? levelFog : presetFog;
        lighting.fogHeightFalloff = 0.08f;
        return lighting;
    }

    bool HandleViewKeys(const Atom::Input& input, Atom::Renderer& renderer, ViewToggles& toggles, Atmosphere* particles)
    {
        Atom::RenderSettings settings = renderer.GetSettings();
        bool lightingChanged = false;

        // F2: render scale 100 -> 85 -> 75 -> 50 -> 100 %.
        if (input.WasKeyPressed(SDL_SCANCODE_F2))
        {
            constexpr float scales[] = { 1.0f, 0.85f, 0.75f, 0.5f };
            std::size_t next = 0;
            for (std::size_t i = 0; i < std::size(scales); ++i)
            {
                if (settings.renderScale >= scales[i] - 0.001f)
                {
                    next = (i + 1) % std::size(scales);
                    break;
                }
            }
            settings.renderScale = scales[next];
            renderer.SetSettings(settings);
        }

        // F5: fog dense -> medium -> light -> off -> the level's.
        if (input.WasKeyPressed(SDL_SCANCODE_F5))
        {
            toggles.fogPreset = (toggles.fogPreset + 1) % std::size(FogPresets);
            lightingChanged = true;
        }

        // F7: post look full -> grade only (no grain/vignette) -> off.
        if (input.WasKeyPressed(SDL_SCANCODE_F7))
        {
            toggles.postMode = (toggles.postMode + 1) % 3;
            const Atom::PostSettings defaults{};
            settings.post = defaults;
            settings.post.enabled = toggles.postMode != 2;
            if (toggles.postMode == 1)
            {
                settings.post.grain = 0.0f;
                settings.post.vignette = 0.0f;
            }
            renderer.SetSettings(settings);
        }

        // F8: particles (leaves, ash, fog banks, rain) on/off.
        if (particles && input.WasKeyPressed(SDL_SCANCODE_F8))
        {
            particles->SetEnabled(!particles->IsEnabled());
        }

        // F3: baked light on/off, to compare with the flat hemisphere ambient.
        if (input.WasKeyPressed(SDL_SCANCODE_F3))
        {
            toggles.bakedLight = !toggles.bakedLight;
            lightingChanged = true;
        }

        // F6: sun shadows on/off.
        if (input.WasKeyPressed(SDL_SCANCODE_F6))
        {
            toggles.shadows = !toggles.shadows;
            lightingChanged = true;
        }

        // F4: MSAA 4x -> 2x -> 1x -> 4x.
        if (input.WasKeyPressed(SDL_SCANCODE_F4))
        {
            settings.msaaSamples = settings.msaaSamples > 1 ? settings.msaaSamples / 2 : 4;
            renderer.SetSettings(settings);
        }
        return lightingChanged;
    }

    bool ApplyViewSwitch(Atom::Renderer& renderer, ViewToggles& toggles, Atom::Camera* camera,
                         Atmosphere* particles, const std::string& what, const std::string& value)
    {
        const bool on = value == "on";
        const bool onOff = on || value == "off";
        char* end = nullptr;
        const float number = std::strtof(value.c_str(), &end);
        const bool isNumber = end && *end == '\0' && !value.empty();
        Atom::RenderSettings settings = renderer.GetSettings();

        if (what == "msaa" && (value == "1" || value == "2" || value == "4"))
        {
            settings.msaaSamples = static_cast<std::uint32_t>(number);
        }
        else if (what == "scale" && isNumber && number >= 0.1f && number <= 1.0f) settings.renderScale = number;
        else if (what == "post" && (value == "full" || value == "grade" || value == "off"))
        {
            toggles.postMode = value == "full" ? 0 : value == "grade" ? 1 : 2;
            settings.post = Atom::PostSettings{};
            settings.post.enabled = toggles.postMode != 2;
            if (toggles.postMode == 1)
            {
                settings.post.grain = 0.0f;
                settings.post.vignette = 0.0f;
            }
        }
        else if (what == "fog")
        {
            const auto found = std::find_if(std::begin(FogPresets), std::end(FogPresets),
                                            [&](const FogPreset& preset) { return value == preset.name; });
            if (found == std::end(FogPresets))
            {
                return false;
            }
            toggles.fogPreset = static_cast<std::size_t>(found - std::begin(FogPresets));
        }
        else if (what == "fov" && camera && isNumber && number >= 10.0f && number <= 150.0f)
        {
            camera->verticalFov = glm::radians(number);
        }
        else if (what == "shadows" && onOff) toggles.shadows = on;
        else if (what == "sun" && onOff) toggles.sun = on;
        else if (what == "glow" && onOff) toggles.glow = on;
        else if (what == "baked" && onOff) toggles.bakedLight = on;
        else if (what == "particles" && particles && onOff) particles->SetEnabled(on);
        else if (what == "water" && onOff) renderer.SetWaterEnabled(on); // M51: for benchmarks
        else if (what == "reflection" && onOff) renderer.SetReflectionEnabled(on);
        else if (what == "rain" && isNumber && number >= 0.0f && number <= 1.0f) toggles.rain = number;
        else if (what == "rain" && value == "level") toggles.rain.reset(); // the environment's again
        else if (what == "weather" && onOff)
        {
            // M51: the lake's weather all at once - water and rain - or none.
            renderer.SetWaterEnabled(on);
            toggles.rain = on ? 1.0f : 0.0f;
        }
        else
        {
            return false;
        }
        renderer.SetSettings(settings);
        return true;
    }
}
