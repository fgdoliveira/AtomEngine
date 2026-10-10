// Developer tools (M41): the demo's own F10 panels - Settings, Spot light,
// Game. The shared ones (Frame, Render, Lighting, Environment, Level) are
// the framework's since M89 (AtomFramework::DevPanels); every widget
// applies through the same Set() switches the test harness uses.
#include "DemoApp.h"

#include <SDL3/SDL_clipboard.h>
#include <imgui.h>

#include <algorithm>
#include <cstdio>
#include <string>

namespace Demo
{
    using namespace AtomFramework; // v0.0.14: the world layer (levels, world, interaction) lives there

    namespace
    {
        bool EditColor(const char* label, glm::vec3& color)
        {
            return ImGui::ColorEdit3(label, &color.x, ImGuiColorEditFlags_Float);
        }

        std::string Vec3Json(const glm::vec3& v)
        {
            char text[96];
            std::snprintf(text, sizeof(text), "[%.3g, %.3g, %.3g]", v.x, v.y, v.z);
            return text;
        }
    }

    void DemoApp::RegisterDevPanels()
    {
        // The shared panels (Frame, Render, Lighting, Environment, Level)
        // are the framework's (M89); these are the demo's own.
        m_devPanels.Add("Settings", 2, [this] { DrawSettingsPanel(); });
        m_devPanels.Add("Spot light", 1, [this] { DrawSpotPanel(); });
        m_devPanels.Add("Game", 0, [this] { DrawGamePanel(); });
    }

    void DemoApp::DrawDevTools(float deltaSeconds)
    {
        DevContext context;
        context.renderer = &GetRenderer();
        context.tools = &GetDevTools();
        context.camera = &m_camera;
        context.view = &m_view;
        context.particles = &m_atmosphere;
        context.levels = m_levels.get();
        context.environment = &m_environment;
        context.presets = OfferedPresets();
        context.environmentName = m_environmentName;
        context.set = [this](const std::string& what, const std::string& value) { return Set(what, value); };
        context.setEnvironment = [this](const std::string& name, float seconds) { return SetEnvironment(name, seconds); };
        context.refreshEnvironment = [this] { RefreshEnvironment(); };
        context.applyLighting = [this] { ApplyLighting(); };
        context.toggles = { { "Unease", "unease", m_unease.IsEnabled() }, { "World", "world", m_drawWorld },
                            { "HUD", "hud", m_showHud } };
        m_devPanels.Draw(context, deltaSeconds);
    }

    void DemoApp::DrawSettingsPanel()
    {
        // Settings (M61): the machine-level choices - GPU preference (saved,
        // needs a restart) and quality (applied at once). Saved per user,
        // except in scripted and --no-settings runs.
        Atom::Renderer& renderer = GetRenderer();
        ImGui::Text("Adapter: %s (%s)", renderer.GetAdapterName().c_str(), renderer.GetBackendName().c_str());
        ImGui::TextDisabled("Running with %s: %s", std::string(ToString(m_resolvedSettings.gpu)).c_str(),
            m_resolvedSettings.gpuReason.c_str());

        int gpu = m_savedSettings.gpu == GpuPreference::HighPerformance ? 1 : 0;
        ImGui::TextUnformatted("GPU preference");
        ImGui::SameLine();
        bool gpuChanged = ImGui::RadioButton("low-power", &gpu, 0);
        ImGui::SameLine();
        gpuChanged |= ImGui::RadioButton("high-performance", &gpu, 1);
        if (gpuChanged)
        {
            m_savedSettings.gpu = gpu == 1 ? GpuPreference::HighPerformance : GpuPreference::LowPower;
            m_savedSettings.pendingFallback.reset(); // an explicit choice clears a fallback
            SaveSettings();
        }
        if (m_savedSettings.gpu != m_resolvedSettings.gpu)
        {
            ImGui::SameLine();
            ImGui::TextColored({ 1.0f, 0.75f, 0.3f, 1.0f }, "restart required");
        }

        static constexpr const char* Modes[] = { "auto", "low", "balanced", "high" };
        int mode = static_cast<int>(m_resolvedSettings.quality);
        if (ImGui::Combo("Quality", &mode, Modes, IM_ARRAYSIZE(Modes)))
        {
            SetQualityMode(static_cast<QualityMode>(mode), true);
        }
        ImGui::Text("Drawing: %s", std::string(ToString(CurrentQualityTier())).c_str());
        if (CurrentQualityTier() == QualityTier::Custom)
        {
            ImGui::SameLine();
            ImGui::TextDisabled("(switches changed by hand; the presets are unchanged)");
        }

        if (m_savedSettings.calibration)
        {
            const CalibrationRecord& c = *m_savedSettings.calibration;
            ImGui::Text("Calibrated: %s on %s (worst p95 %.1f ms)%s", std::string(ToString(c.tier)).c_str(),
                c.adapter.c_str(), c.p95Ms, IsCalibrationValid(c, renderer.GetAdapterName()) ? "" : " - not for this adapter");
        }
        else
        {
            ImGui::TextDisabled("Not calibrated (Auto draws High)");
        }
        if (m_calibration.active)
        {
            ImGui::TextColored({ 0.5f, 0.9f, 1.0f, 1.0f }, "Calibrating: view %zu of 2, pass %zu of 6 - hands off",
                m_calibration.scene + 1, m_calibration.tier + 1);
        }
        else
        {
            if (ImGui::Button("Calibrate now"))
            {
                StartCalibration(false);
            }
            ImGui::SameLine();
            if (ImGui::Checkbox("Calibrate next launch", &m_savedSettings.calibrateNextLaunch))
            {
                SaveSettings();
            }
            ImGui::TextDisabled("Plugged in, window on the laptop's own screen, about a minute.");
        }
        if (ImGui::Button("Reset to defaults"))
        {
            m_savedSettings = GameSettings{};
            SaveSettings();
            SetQualityMode(QualityMode::High, false);
        }
        ImGui::SameLine();
        if (m_settingsPersist)
        {
            ImGui::TextDisabled("%s", m_settingsPath.c_str());
        }
        else
        {
            ImGui::TextDisabled("not saved this run (test or --no-settings)");
        }
    }

    void DemoApp::DrawSpotPanel()
    {
        // Spot light (M42): the renderer's spot, before the flashlight
        // exists. Every parameter live; Copy as JSON for the data later.
        const Atom::FrameStats& stats = GetRenderer().GetLastFrameStats();
        const bool flashlight = m_flashlight.IsOwned() && !m_devSpotOn;
        ImGui::TextDisabled(flashlight ? "Editing: the flashlight (F)" : "Editing: the test spot");
        ImGui::Checkbox("Test spot", &m_devSpotOn);
        ImGui::SameLine();
        ImGui::Checkbox("Follow the camera", &m_devSpotFollows);
        Atom::SpotLight& spot = flashlight ? m_flashlight.EditLight() : m_devSpot;
        ImGui::SliderFloat("Range (m)", &spot.range, 1.0f, 40.0f);
        ImGui::SliderFloat("Inner angle", &spot.innerAngleDegrees, 1.0f, 60.0f, "%.1f deg");
        ImGui::SliderFloat("Outer angle", &spot.outerAngleDegrees, 1.0f, 75.0f, "%.1f deg");
        spot.outerAngleDegrees = std::max(spot.outerAngleDegrees, spot.innerAngleDegrees);
        ImGui::SliderFloat("Intensity", &spot.intensity, 0.0f, 40.0f);
        EditColor("Colour", spot.color);
        ImGui::SliderFloat("Specular", &spot.specular, 0.0f, 4.0f);
        ImGui::SliderFloat("Beam in the air", &spot.beam, 0.0f, 0.5f); // M45
        // Shadow bias (M43): too little and surfaces shadow themselves in
        // stripes (acne); too much and shadows float off their casters
        // (peter-panning). Per metre from the lamp.
        ImGui::Checkbox("Casts shadows", &spot.castsShadows);
        ImGui::SliderFloat("Normal offset /m", &spot.shadowNormalOffset, 0.0f, 0.03f, "%.4f");
        ImGui::Text("Spot shadow draws %u", stats.spotShadowDrawn);
        if (!flashlight && !m_devSpotFollows)
        {
            ImGui::DragFloat3("Position", &spot.position.x, 0.05f);
            ImGui::DragFloat3("Direction", &spot.direction.x, 0.01f, -1.0f, 1.0f);
        }
        if (flashlight && ImGui::Button("Copy as JSON"))
        {
            // Exactly Assets/Data/flashlight.json (M46).
            SDL_SetClipboardText(m_flashlight.SaveSettings().c_str());
        }
        if (flashlight)
        {
            ImGui::SameLine();
            ImGui::TextDisabled("(paste into Assets/Data/flashlight.json)");
        }
        else if (ImGui::Button("Copy as JSON"))
        {
            char json[400];
            std::snprintf(json, sizeof(json),
                "\"spot\": { \"range\": %.3g, \"inner\": %.3g, \"outer\": %.3g, \"intensity\": %.3g, "
                "\"color\": %s, \"specular\": %.3g, \"shadows\": %s, \"shadowNormalOffset\": %.3g }",
                spot.range, spot.innerAngleDegrees, spot.outerAngleDegrees, spot.intensity,
                Vec3Json(spot.color).c_str(), spot.specular, spot.castsShadows ? "true" : "false",
                spot.shadowNormalOffset);
            SDL_SetClipboardText(json);
        }
    }

    void DemoApp::DrawGamePanel()
    {
        // Game: the demo's state - mode, flags, counters.
        const glm::vec3 feet = FeetPosition();
        ImGui::Text("mode %s   feet %.2f %.2f %.2f", ModeName().c_str(), feet.x, feet.y, feet.z);
        if (ImGui::CollapsingHeader("Flags"))
        {
            for (const std::string& flag : m_gameState.GetFlags())
            {
                ImGui::BulletText("%s", flag.c_str());
            }
        }
        if (ImGui::CollapsingHeader("Counters"))
        {
            for (const auto& [name, value] : m_gameState.GetCounters())
            {
                int edited = value;
                if (ImGui::InputInt(name.c_str(), &edited))
                {
                    m_gameState.SetCounter(name, edited);
                }
            }
        }
    }
}
