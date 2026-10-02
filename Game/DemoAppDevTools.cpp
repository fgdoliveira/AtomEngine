// Developer tools (M41): DemoApp's ImGui panels. F10 shows them; the engine
// (Atom::DevTools) runs ImGui and draws it last, outside every capture.
//
// Immediate mode, as ImGui is meant to be used: nothing here keeps widget
// objects. Each frame the panels are described again from the game's
// current state, and a widget's return value says the user changed it -
// which is applied at once (through the same Set() switches the test
// harness uses, so both paths stay one).
#include "DemoApp.h"

#include <SDL3/SDL_clipboard.h>
#include <imgui.h>

#include <cstdio>
#include <string>

namespace AtomGame
{
    namespace
    {
        // The names Set("fog") accepts, densest first.
        constexpr const char* FogNames[] = { "dense", "medium", "light", "off" };
        constexpr const char* PostNames[] = { "full", "grade", "off" };

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

    void DemoApp::DrawDevTools(float deltaSeconds)
    {
        m_frameHistory[m_frameHistoryNext] = deltaSeconds * 1000.0f;
        m_frameHistoryNext = (m_frameHistoryNext + 1) % m_frameHistory.size();
        if (!GetDevTools().IsFrameActive())
        {
            return;
        }
        // ImGui's two closing rules, which differ (mixing them up is the
        // classic ImGui crash, "Missing End()"):
        //   Begin()      -> End() ALWAYS, even when Begin() returned false
        //                   (the window is collapsed or clipped: skip only
        //                   the contents);
        //   BeginTable() -> EndTable() ONLY when BeginTable() returned true.
        const float scale = 1.0f;
        // Laid out around the window's edges, clear of the game's HUD in
        // the top-left corner: stats and level on the left, render and
        // lighting on the right.
        const float width = ImGui::GetIO().DisplaySize.x;
        Atom::Renderer& renderer = GetRenderer();
        const Atom::FrameStats& stats = renderer.GetLastFrameStats();
        // The harness can collapse or expand every panel for a frame
        // ("set devtools_collapsed"), to run the collapsed path in tests.
        const auto collapse = [&] {
            if (m_devToolsCollapse)
            {
                ImGui::SetNextWindowCollapsed(*m_devToolsCollapse, ImGuiCond_Always);
            }
        };

        // Frame: time and what was drawn.
        ImGui::SetNextWindowPos({ 16.0f, 130.0f }, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize({ 340.0f * scale, 260.0f * scale }, ImGuiCond_FirstUseEver);
        collapse();
        if (ImGui::Begin("Frame"))
        {
            float average = 0.0f;
            float worst = 0.0f;
            for (const float ms : m_frameHistory)
            {
                average += ms;
                worst = std::max(worst, ms);
            }
            average /= static_cast<float>(m_frameHistory.size());
            ImGui::Text("%.2f ms average (%.0f fps), worst %.2f ms", average,
                average > 0.0f ? 1000.0f / average : 0.0f, worst);
            ImGui::PlotLines("##frames", m_frameHistory.data(), static_cast<int>(m_frameHistory.size()),
                static_cast<int>(m_frameHistoryNext), "frame time (ms)", 0.0f, std::max(20.0f, worst),
                { -1.0f, 60.0f });
            ImGui::Text("Scene %ux%u  MSAA %ux", stats.sceneWidth, stats.sceneHeight, stats.msaaSamples);
            ImGui::Text("Draws %u / %u   shadow %u   spot shadow %u", stats.drawn, stats.submitted,
                stats.shadowDrawn, stats.spotShadowDrawn);
            ImGui::Text("Binds: pipelines %u, materials %u", stats.pipelineBinds, stats.materialBinds);
            // Light culling (M46): the draws each kind of light still shades.
            ImGui::Text("Lit draws: spot %u, live %u (of %u lights)", stats.spotLitDraws, stats.liveLitDraws,
                stats.liveLights);
            ImGui::Text("Particles %u   render textures %u", stats.particles, stats.renderTextures);
            if (ImGui::BeginTable("layers", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_SizingStretchSame))
            {
                ImGui::TableSetupColumn("Layer");
                ImGui::TableSetupColumn("Chunks");
                ImGui::TableSetupColumn("Draws");
                ImGui::TableSetupColumn("Shadow");
                ImGui::TableHeadersRow();
                const char* names[] = { "Near", "Mid", "Far" };
                for (std::size_t i = 0; i < Atom::RenderLayerCount; ++i)
                {
                    const Atom::LayerStats& layer = stats.layers[i];
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn(); ImGui::TextUnformatted(names[i]);
                    ImGui::TableNextColumn(); ImGui::Text("%u/%u", layer.chunksVisible, layer.chunks);
                    ImGui::TableNextColumn(); ImGui::Text("%u", layer.drawn);
                    ImGui::TableNextColumn(); ImGui::Text("%u", layer.shadowDrawn);
                }
                ImGui::EndTable();
            }
        }
        ImGui::End();

        // Render: the same switches as the harness's "set".
        ImGui::SetNextWindowPos({ width - 330.0f, 16.0f }, ImGuiCond_FirstUseEver);
        collapse();
        if (ImGui::Begin("Render"))
        {
            const Atom::RenderSettings& settings = renderer.GetSettings();
            int msaa = settings.msaaSamples >= 4 ? 2 : settings.msaaSamples == 2 ? 1 : 0;
            if (ImGui::Combo("MSAA", &msaa, "1x\0" "2x\0" "4x\0"))
            {
                Set("msaa", msaa == 2 ? "4" : msaa == 1 ? "2" : "1");
            }
            float renderScale = settings.renderScale;
            if (ImGui::SliderFloat("Render scale", &renderScale, 0.25f, 1.0f, "%.2f"))
            {
                Set("scale", std::to_string(renderScale));
            }
            int fog = static_cast<int>(m_fogPreset);
            if (ImGui::Combo("Fog", &fog, FogNames, IM_ARRAYSIZE(FogNames)))
            {
                Set("fog", FogNames[fog]);
            }
            int post = m_postMode;
            if (ImGui::Combo("Post", &post, PostNames, IM_ARRAYSIZE(PostNames)))
            {
                Set("post", PostNames[post]);
            }
            float fov = glm::degrees(m_camera.verticalFov);
            if (ImGui::SliderFloat("FOV", &fov, 30.0f, 110.0f, "%.0f deg"))
            {
                Set("fov", std::to_string(fov));
            }
            const auto toggle = [&](const char* label, const char* what, bool value) {
                if (ImGui::Checkbox(label, &value))
                {
                    Set(what, value ? "on" : "off");
                }
            };
            toggle("Shadows", "shadows", m_shadowsEnabled);
            toggle("Sun", "sun", m_sunEnabled);
            toggle("Particles", "particles", m_atmosphere.IsEnabled());
            toggle("Unease", "unease", m_unease.IsEnabled());
            toggle("World", "world", m_drawWorld);
            toggle("HUD", "hud", m_showHud);
            toggle("F1 overlay", "overlay", m_showDebugOverlay);
            if (ImGui::Checkbox("Baked light (F3)", &m_bakedLightEnabled))
            {
                ApplyLighting();
            }
        }
        ImGui::End();

        // Lighting: the level's own values, live. "Copy as JSON" gives the
        // block to paste into the level file; nothing is saved by itself.
        Level* level = m_levels ? m_levels->GetLevel() : nullptr;
        if (level)
        {
            ImGui::SetNextWindowPos({ width - 420.0f, 330.0f }, ImGuiCond_FirstUseEver);
            collapse();
            if (ImGui::Begin("Lighting"))
            {
                LevelLighting& l = level->EditLighting();
                bool changed = false;
                changed |= ImGui::DragFloat3("Sun direction", &l.sunDirection.x, 0.01f, -1.0f, 1.0f);
                changed |= EditColor("Sun", l.sunColor);
                changed |= EditColor("Sky", l.skyColor);
                changed |= EditColor("Ground", l.groundColor);
                changed |= EditColor("Fog colour", l.fogColor);
                changed |= ImGui::Checkbox("Sun shadows", &l.shadows);
                changed |= ImGui::SliderFloat("Baked light", &l.bakedLight, 0.0f, 1.0f);
                changed |= ImGui::SliderFloat("Glow strength", &l.glowStrength, 0.0f, 2.0f);
                changed |= ImGui::SliderFloat("Glow threshold", &l.glowThreshold, 0.1f, 4.0f);
                if (changed)
                {
                    ApplyLighting();
                }
                if (ImGui::Button("Copy as JSON"))
                {
                    char glow[96];
                    std::snprintf(glow, sizeof(glow), "{ \"strength\": %.3g, \"threshold\": %.3g }",
                        l.glowStrength, l.glowThreshold);
                    char baked[32];
                    std::snprintf(baked, sizeof(baked), "%.3g", l.bakedLight);
                    const std::string json = std::string("\"lighting\": {\n")
                        + "  \"sunDirection\": " + Vec3Json(l.sunDirection) + ",\n"
                        + "  \"sunColor\": " + Vec3Json(l.sunColor) + ",\n"
                        + "  \"skyColor\": " + Vec3Json(l.skyColor) + ",\n"
                        + "  \"groundColor\": " + Vec3Json(l.groundColor) + ",\n"
                        + "  \"fogColor\": " + Vec3Json(l.fogColor) + ",\n"
                        + "  \"shadows\": " + (l.shadows ? "true" : "false") + ",\n"
                        + "  \"bakedLight\": " + baked + ",\n"
                        + "  \"glow\": " + glow + "\n}";
                    SDL_SetClipboardText(json.c_str());
                }
                ImGui::SameLine();
                ImGui::TextDisabled("(paste into %s.json)", level->GetName().c_str());
            }
            ImGui::End(); // always, even when Begin returned false (collapsed)
        }

        // Spot light (M42): the renderer's spot, before the flashlight
        // exists. Every parameter live; Copy as JSON for the data later.
        ImGui::SetNextWindowPos({ width - 420.0f, 600.0f }, ImGuiCond_FirstUseEver);
        collapse();
        if (ImGui::Begin("Spot light"))
        {
            // The flashlight once found (M45); before that, or with the test
            // spot switched on, the test spot.
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
        ImGui::End();

        // Level: where we are, what's in it, the game's state.
        ImGui::SetNextWindowPos({ 16.0f, 400.0f }, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize({ 340.0f * scale, 280.0f * scale }, ImGuiCond_FirstUseEver);
        collapse();
        if (ImGui::Begin("Level"))
        {
            const glm::vec3 feet = FeetPosition();
            ImGui::Text("%s   mode %s", level ? level->GetName().c_str() : "-", ModeName().c_str());
            ImGui::Text("Feet %.2f %.2f %.2f", feet.x, feet.y, feet.z);
            if (ImGui::CollapsingHeader("Entities", ImGuiTreeNodeFlags_DefaultOpen))
            {
                if (GameWorld* world = CurrentWorld();
                    world && ImGui::BeginTable("entities", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_ScrollY,
                                                { 0.0f, 140.0f }))
                {
                    ImGui::TableSetupColumn("Name");
                    ImGui::TableSetupColumn("Position");
                    ImGui::TableSetupColumn("Capabilities");
                    ImGui::TableHeadersRow();
                    world->ForEach([&](EntityId, const Entity& entity) {
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn(); ImGui::TextUnformatted(entity.name.c_str());
                        ImGui::TableNextColumn();
                        ImGui::Text("%.1f %.1f %.1f", entity.position.x, entity.position.y, entity.position.z);
                        ImGui::TableNextColumn();
                        ImGui::Text("%s%s%s%s%s", entity.renderable ? "drawn " : "",
                            entity.interactable ? "use " : "", entity.animated ? "anim " : "",
                            entity.animator ? "animator " : "", entity.hidden ? "hidden" : "");
                    });
                    ImGui::EndTable();
                }
            }
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
        ImGui::End();
        m_devToolsCollapse.reset(); // applied once
    }
}
