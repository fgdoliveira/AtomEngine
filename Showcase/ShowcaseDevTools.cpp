// The Showcase's F10 tools (v0.0.14, M89): the framework's shared panels
// (Frame, Render, Lighting, Environment, Level), plus two of its own -
// Features, the lens's switches, and Clock, the village's time of day.
// Every widget applies through Set(), as the scenarios do.
#include "ShowcaseApp.h"

#include <imgui.h>

#include <string>

namespace Showcase
{
    void ShowcaseApp::RegisterDevPanels()
    {
        m_devPanels.Add("Features", 2, [this] {
            const std::vector<FeatureInfo>& catalog = FeatureCatalog();
            for (std::size_t i = 0; i < catalog.size(); ++i)
            {
                bool on = m_featureOn[i];
                if (ImGui::Checkbox(catalog[i].title, &on))
                {
                    Set(std::string("feature_") + catalog[i].id, on ? "on" : "off");
                }
                ImGui::SameLine(170.0f);
                ImGui::TextDisabled("%s  (%s)", FeatureCost(static_cast<int>(i)).c_str(), catalog[i].source);
            }
        });
        m_devPanels.Add("Clock", 1, [this] {
            ImGui::Text("Now: %s", EnvironmentName().c_str());
            bool running = m_clockOn;
            if (ImGui::Checkbox("Time passes (T)", &running))
            {
                Set("clock", running ? "on" : "off");
            }
            ImGui::SameLine();
            if (ImGui::Button("Next (P)"))
            {
                Set("clock", "next");
            }
            ImGui::SliderFloat("Hold", &m_holdSeconds, 1.0f, 120.0f, "%.0f s");
            ImGui::ProgressBar(m_environment.IsTransitioning() ? 1.0f : m_clockTime / m_holdSeconds,
                               { -1.0f, 0.0f }, m_environment.IsTransitioning() ? "blending" : "holding");
        });
    }

    void ShowcaseApp::DrawDevTools(float deltaSeconds)
    {
        DevContext context;
        context.renderer = &GetRenderer();
        context.tools = &GetDevTools();
        context.camera = &m_camera;
        context.view = &m_view;
        context.particles = &m_atmosphere;
        context.levels = m_levels.get();
        context.environment = &m_environment;
        context.presets = m_presets.Offered(m_levels->GetLevel());
        context.environmentName = m_environmentName;
        context.set = [this](const std::string& what, const std::string& value) { return Set(what, value); };
        context.setEnvironment = [this](const std::string& name, float seconds) { return SetEnvironment(name, seconds); };
        context.refreshEnvironment = [this] {
            m_environment.Reset(m_presets.Resolve(m_levels->GetLevel(), m_environmentName));
        };
        context.applyLighting = [this] { ApplyLighting(); };
        context.toggles = { { "World", "world", m_drawWorld }, { "HUD", "hud", m_showHud }, { "Lens (Tab)", "lens", m_lens } };
        m_devPanels.Draw(context, deltaSeconds);
    }
}
