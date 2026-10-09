// The developer tools' shared panels (v0.0.14, M89): the demo's F10 panels
// (M41-M49) moved to the framework, so every app has them.
//
// ImGui's two closing rules, which differ (mixing them up is the classic
// ImGui crash, "Missing End()"):
//   Begin()      -> End() ALWAYS, even when Begin() returned false (the
//                   window is collapsed or clipped: skip only the contents);
//   BeginTable() -> EndTable() ONLY when BeginTable() returned true.
#include "Debug/DevPanels.h"

#include "Environment/Atmosphere.h"
#include "Environment/EnvironmentController.h"
#include "Level/Level.h"
#include "Level/LevelManager.h"
#include "Level/ViewToggles.h"

#include "Debug/DevTools.h"
#include "Renderer/Renderer.h"
#include "Scene/Camera.h"

#include <SDL3/SDL_clipboard.h>
#include <glm/trigonometric.hpp>
#include <imgui.h>

#include <algorithm>
#include <cstdio>
#include <iterator>

namespace AtomFramework
{
    namespace
    {
        constexpr const char* PostNames[] = { "full", "grade", "off" };
        constexpr float ColumnWidth = 360.0f;
        constexpr float Margin = 16.0f;

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

    void DevPanels::Add(std::string name, int column, std::function<void()> draw)
    {
        m_panels.push_back({ std::move(name), std::clamp(column, 0, 2), std::move(draw) });
    }

    bool DevPanels::Apply(const DevContext& context, const std::string& what, const std::string& value)
    {
        if (context.set)
        {
            return context.set(what, value);
        }
        ViewToggles fallback;
        const bool applied = ApplyViewSwitch(*context.renderer, context.view ? *context.view : fallback,
                                             context.camera, context.particles, what, value);
        if (applied && context.applyLighting)
        {
            context.applyLighting();
        }
        return applied;
    }

    bool DevPanels::Begin(const char* name, int column, float width, float height)
    {
        // Three columns: left and right down the window's edges, the centre
        // along its top - placed once (FirstUseEver), then wherever the user
        // drags them.
        const ImVec2 display = ImGui::GetIO().DisplaySize;
        const float x = column == 0 ? Margin : column == 1 ? display.x - width - Margin : display.x * 0.5f - width * 0.5f;
        ImGui::SetNextWindowPos({ x, m_columnY[column] }, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize({ width, height }, ImGuiCond_FirstUseEver);
        m_columnY[column] += height + Margin * 0.5f;
        if (m_collapse)
        {
            ImGui::SetNextWindowCollapsed(*m_collapse, ImGuiCond_Always);
        }
        ++m_drawn;
        return ImGui::Begin(name);
    }

    void DevPanels::Draw(const DevContext& context, float deltaSeconds)
    {
        m_frameHistory[m_frameHistoryNext] = deltaSeconds * 1000.0f;
        m_frameHistoryNext = (m_frameHistoryNext + 1) % m_frameHistory.size();
        m_drawn = 0;
        // An ImGui frame also runs for the F1 overlay alone; the panels are F10's.
        if (!context.renderer || !context.tools || !context.tools->IsFrameActive() || !context.tools->IsVisible())
        {
            return;
        }
        // The left column starts below the games' HUD corner (and the F1 overlay).
        m_columnY = { 130.0f, Margin, Margin };
        DrawFrame(context);
        DrawRender(context);
        DrawEnvironment(context);
        DrawLighting(context);
        DrawLevel(context);
        for (const Panel& panel : m_panels)
        {
            if (Begin(panel.name.c_str(), panel.column, ColumnWidth, 220.0f))
            {
                panel.draw();
            }
            ImGui::End();
        }
        m_collapse.reset(); // applied once
    }

    void DevPanels::DrawFrame(const DevContext& context)
    {
        // Frame: time and what was drawn.
        const Atom::FrameStats& stats = context.renderer->GetLastFrameStats();
        if (Begin("Frame", 0, ColumnWidth, 270.0f))
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
    }

    void DevPanels::DrawRender(const DevContext& context)
    {
        // Render: the same switches as the harness's "set".
        if (Begin("Render", 1, ColumnWidth - 30.0f, 300.0f))
        {
            const Atom::RenderSettings& settings = context.renderer->GetSettings();
            int msaa = settings.msaaSamples >= 4 ? 2 : settings.msaaSamples == 2 ? 1 : 0;
            if (ImGui::Combo("MSAA", &msaa, "1x\0" "2x\0" "4x\0"))
            {
                Apply(context, "msaa", msaa == 2 ? "4" : msaa == 1 ? "2" : "1");
            }
            float renderScale = settings.renderScale;
            if (ImGui::SliderFloat("Render scale", &renderScale, 0.25f, 1.0f, "%.2f"))
            {
                Apply(context, "scale", std::to_string(renderScale));
            }
            if (context.camera)
            {
                float fov = glm::degrees(context.camera->verticalFov);
                if (ImGui::SliderFloat("FOV", &fov, 30.0f, 110.0f, "%.0f deg"))
                {
                    Apply(context, "fov", std::to_string(fov));
                }
            }
            const auto toggle = [&](const char* label, const char* what, bool value) {
                if (ImGui::Checkbox(label, &value))
                {
                    Apply(context, what, value ? "on" : "off");
                }
            };
            if (const ViewToggles* view = context.view)
            {
                int fog = static_cast<int>(view->fogPreset);
                const char* fogNames[std::size(FogPresets)];
                for (std::size_t i = 0; i < std::size(FogPresets); ++i)
                {
                    fogNames[i] = FogPresets[i].name;
                }
                if (ImGui::Combo("Fog", &fog, fogNames, static_cast<int>(std::size(fogNames))))
                {
                    Apply(context, "fog", fogNames[fog]);
                }
                int post = view->postMode;
                if (ImGui::Combo("Post", &post, PostNames, IM_ARRAYSIZE(PostNames)))
                {
                    Apply(context, "post", PostNames[post]);
                }
                toggle("Shadows", "shadows", view->shadows);
                toggle("Sun", "sun", view->sun);
                toggle("Glow", "glow", view->glow);
                toggle("Baked light (F3)", "baked", view->bakedLight);
            }
            if (context.particles)
            {
                toggle("Particles", "particles", context.particles->IsEnabled());
            }
            for (const DevContext::Toggle& extra : context.toggles)
            {
                toggle(extra.label, extra.what, extra.on);
            }
            toggle("F1 overlay", "overlay", context.tools->IsOverlayVisible());
        }
        ImGui::End();
    }

    void DevPanels::DrawLighting(const DevContext& context)
    {
        // Lighting: the level's own values, live. "Copy as JSON" gives the
        // block to paste into the level file; nothing is saved by itself.
        Level* level = context.levels ? context.levels->GetLevel() : nullptr;
        if (!level)
        {
            return;
        }
        if (Begin("Lighting", 1, ColumnWidth + 60.0f, 330.0f))
        {
            LevelLighting& l = level->EditLighting();
            bool changed = false;
            changed |= ImGui::DragFloat3("Sun direction", &l.sunDirection.x, 0.01f, -1.0f, 1.0f);
            changed |= EditColor("Sun", l.sunColor);
            changed |= EditColor("Sky", l.skyColor);
            changed |= EditColor("Ground", l.groundColor);
            changed |= EditColor("Fog colour", l.fogColor);
            // M47: the level's fog density (used while Render > Fog is "level").
            bool hasFog = l.fogDensity.has_value();
            if (ImGui::Checkbox("Level fog", &hasFog))
            {
                l.fogDensity = hasFog ? std::optional<float>(0.03f) : std::nullopt;
                changed = true;
            }
            if (l.fogDensity)
            {
                ImGui::SameLine();
                changed |= ImGui::SliderFloat("##density", &*l.fogDensity, 0.0f, 0.15f, "%.3f");
            }
            // M47: the day sky.
            bool hasSky = l.sky.has_value();
            if (ImGui::Checkbox("Sky gradient", &hasSky))
            {
                l.sky = hasSky ? std::optional<SkyGradient>(SkyGradient{}) : std::nullopt;
                changed = true;
            }
            if (l.sky)
            {
                changed |= EditColor("Zenith", l.sky->zenith);
                changed |= EditColor("Horizon", l.sky->horizon);
                changed |= ImGui::SliderFloat("Sun size", &l.sky->sunSize, 0.0f, 10.0f, "%.2f deg");
                changed |= ImGui::SliderFloat("Sun glow", &l.sky->sunGlow, 0.0f, 2.0f);
            }
            // M48: how the level's water looks.
            if (ImGui::TreeNode("Water"))
            {
                changed |= EditColor("Shallow", l.water.shallow);
                changed |= EditColor("Deep", l.water.deep);
                changed |= ImGui::SliderFloat("Sky reflection", &l.water.skyReflection, 0.0f, 1.0f);
                changed |= ImGui::SliderFloat("Ripple", &l.water.ripple, 0.0f, 3.0f);
                changed |= ImGui::SliderFloat("Glint", &l.water.glint, 0.0f, 2.0f);
                changed |= ImGui::Checkbox("Reflection", &l.water.reflection);
                ImGui::TreePop(); // only when TreeNode returned true
            }
            changed |= ImGui::Checkbox("Sun shadows", &l.shadows);
            changed |= ImGui::SliderFloat("Baked light", &l.bakedLight, 0.0f, 1.0f);
            changed |= ImGui::SliderFloat("Glow strength", &l.glowStrength, 0.0f, 2.0f);
            changed |= ImGui::SliderFloat("Glow threshold", &l.glowThreshold, 0.1f, 4.0f);
            if (changed)
            {
                if (context.refreshEnvironment)
                {
                    context.refreshEnvironment(); // the preset showing, over the edited level
                }
                if (context.applyLighting)
                {
                    context.applyLighting();
                }
            }
            if (ImGui::Button("Copy as JSON"))
            {
                char glow[96];
                std::snprintf(glow, sizeof(glow), "{ \"strength\": %.3g, \"threshold\": %.3g }",
                              l.glowStrength, l.glowThreshold);
                char baked[32];
                std::snprintf(baked, sizeof(baked), "%.3g", l.bakedLight);
                std::string extra;
                if (l.fogDensity)
                {
                    char density[48];
                    std::snprintf(density, sizeof(density), "  \"fogDensity\": %.3g,\n", *l.fogDensity);
                    extra += density;
                }
                if (l.sky)
                {
                    char sun[96];
                    std::snprintf(sun, sizeof(sun), "\"sunSize\": %.3g, \"sunGlow\": %.3g", l.sky->sunSize, l.sky->sunGlow);
                    extra += "  \"skyGradient\": { \"zenith\": " + Vec3Json(l.sky->zenith)
                        + ", \"horizon\": " + Vec3Json(l.sky->horizon) + ", " + sun + " },\n";
                }
                char water[160];
                std::snprintf(water, sizeof(water), "\"skyReflection\": %.3g, \"ripple\": %.3g, \"glint\": %.3g%s",
                              l.water.skyReflection, l.water.ripple, l.water.glint,
                              l.water.reflection ? ", \"reflection\": true" : "");
                extra += "  \"water\": { \"shallow\": " + Vec3Json(l.water.shallow)
                    + ", \"deep\": " + Vec3Json(l.water.deep) + ", " + water + " },\n";
                const std::string json = std::string("\"lighting\": {\n")
                    + "  \"sunDirection\": " + Vec3Json(l.sunDirection) + ",\n"
                    + "  \"sunColor\": " + Vec3Json(l.sunColor) + ",\n"
                    + "  \"skyColor\": " + Vec3Json(l.skyColor) + ",\n"
                    + "  \"groundColor\": " + Vec3Json(l.groundColor) + ",\n"
                    + "  \"fogColor\": " + Vec3Json(l.fogColor) + ",\n"
                    + extra
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

    void DevPanels::DrawEnvironment(const DevContext& context)
    {
        // Environment (M49): switch presets, blended; edit what's showing
        // live; Copy as JSON gives a whole preset file.
        if (!context.environment || !context.levels || !context.levels->GetLevel())
        {
            return;
        }
        EnvironmentController& environment = *context.environment;
        if (Begin("Environment", 0, ColumnWidth, 300.0f))
        {
            ImGui::SliderFloat("Transition", &m_environmentSeconds, 0.0f, 10.0f, "%.1f s");
            if (ImGui::RadioButton("level", context.environmentName.empty()) && context.setEnvironment)
            {
                context.setEnvironment("level", m_environmentSeconds);
            }
            for (const std::string& name : context.presets)
            {
                ImGui::SameLine();
                if (ImGui::GetContentRegionAvail().x < 90.0f)
                {
                    ImGui::NewLine();
                }
                if (ImGui::RadioButton(name.c_str(), context.environmentName == name) && context.setEnvironment)
                {
                    context.setEnvironment(name, m_environmentSeconds);
                }
            }
            if (environment.IsTransitioning())
            {
                ImGui::ProgressBar(environment.Progress(), { -1.0f, 0.0f }, "blending");
            }

            ImGui::SeparatorText("Showing now (live edits last until the next switch)");
            EnvironmentState e = environment.Current();
            bool changed = false;
            changed |= ImGui::DragFloat3("Sun direction", &e.sunDirection.x, 0.01f, -1.0f, 1.0f);
            changed |= EditColor("Sun", e.sunColor);
            changed |= EditColor("Ambient sky", e.skyColor);
            changed |= EditColor("Ambient ground", e.groundColor);
            changed |= EditColor("Fog colour", e.fogColor);
            if (e.fogDensity)
            {
                changed |= ImGui::SliderFloat("Fog density", &*e.fogDensity, 0.0f, 0.15f, "%.3f");
            }
            if (e.sky)
            {
                changed |= EditColor("Zenith", e.sky->zenith);
                changed |= EditColor("Horizon", e.sky->horizon);
                changed |= ImGui::SliderFloat("Sun size", &e.sky->sunSize, 0.0f, 10.0f, "%.2f deg");
                changed |= ImGui::SliderFloat("Sun glow", &e.sky->sunGlow, 0.0f, 2.0f);
            }
            changed |= EditColor("Water shallow", e.water.shallow);
            changed |= EditColor("Water deep", e.water.deep);
            changed |= ImGui::SliderFloat("Sky reflection", &e.water.skyReflection, 0.0f, 1.0f);
            changed |= ImGui::SliderFloat("Ripple", &e.water.ripple, 0.0f, 3.0f);
            changed |= ImGui::SliderFloat("Glint", &e.water.glint, 0.0f, 2.0f);
            changed |= ImGui::Checkbox("Water reflection", &e.water.reflection);
            changed |= ImGui::SliderFloat("Rain", &e.rain, 0.0f, 1.0f);
            changed |= ImGui::DragFloat3("Wind (m/s)", &e.wind.x, 0.02f, -6.0f, 6.0f);
            if (changed)
            {
                environment.Reset(e);
                if (context.applyLighting)
                {
                    context.applyLighting();
                }
            }
            if (ImGui::Button("Copy as JSON"))
            {
                const std::string name = context.environmentName.empty() ? "my_preset" : context.environmentName;
                std::string json = "{\n  \"$schema\": \"../../Framework/Schemas/environment.schema.json\",\n"
                    "  \"name\": \"" + name + "\",\n"
                    "  \"sunDirection\": " + Vec3Json(e.sunDirection) + ",\n"
                    "  \"sunColor\": " + Vec3Json(e.sunColor) + ",\n"
                    "  \"skyColor\": " + Vec3Json(e.skyColor) + ",\n"
                    "  \"groundColor\": " + Vec3Json(e.groundColor) + ",\n"
                    "  \"fogColor\": " + Vec3Json(e.fogColor) + ",\n";
                char number[160];
                if (e.fogDensity)
                {
                    std::snprintf(number, sizeof(number), "  \"fogDensity\": %.3g,\n", *e.fogDensity);
                    json += number;
                }
                if (e.sky)
                {
                    std::snprintf(number, sizeof(number), "\"sunSize\": %.3g, \"sunGlow\": %.3g",
                                  e.sky->sunSize, e.sky->sunGlow);
                    json += "  \"skyGradient\": { \"zenith\": " + Vec3Json(e.sky->zenith)
                        + ", \"horizon\": " + Vec3Json(e.sky->horizon) + ", " + number + " },\n";
                }
                std::snprintf(number, sizeof(number), "\"skyReflection\": %.3g, \"ripple\": %.3g, \"glint\": %.3g%s",
                              e.water.skyReflection, e.water.ripple, e.water.glint,
                              e.water.reflection ? ", \"reflection\": true" : "");
                json += "  \"water\": { \"shallow\": " + Vec3Json(e.water.shallow)
                    + ", \"deep\": " + Vec3Json(e.water.deep) + ", " + number + " }\n}\n";
                SDL_SetClipboardText(json.c_str());
            }
            ImGui::SameLine();
            ImGui::TextDisabled("(Content/Environments/<name>.json)");
        }
        ImGui::End(); // always, even when Begin returned false (collapsed)
    }

    void DevPanels::DrawLevel(const DevContext& context)
    {
        // Level: where we are and what's in it.
        Level* level = context.levels ? context.levels->GetLevel() : nullptr;
        if (!level)
        {
            return;
        }
        if (Begin("Level", 0, ColumnWidth, 240.0f))
        {
            ImGui::TextUnformatted(level->GetName().c_str());
            if (context.camera)
            {
                const glm::vec3 eye = context.camera->GetPosition();
                ImGui::SameLine();
                ImGui::TextDisabled("  eye %.2f %.2f %.2f", eye.x, eye.y, eye.z);
            }
            if (ImGui::BeginTable("entities", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_ScrollY, { 0.0f, 160.0f }))
            {
                ImGui::TableSetupColumn("Name");
                ImGui::TableSetupColumn("Position");
                ImGui::TableSetupColumn("Capabilities");
                ImGui::TableHeadersRow();
                level->GetWorld().ForEach([&](EntityId, const Entity& entity) {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn(); ImGui::TextUnformatted(entity.name.c_str());
                    ImGui::TableNextColumn();
                    ImGui::Text("%.1f %.1f %.1f", entity.position.x, entity.position.y, entity.position.z);
                    ImGui::TableNextColumn();
                    ImGui::Text("%s%s%s%s%s", entity.renderable ? "drawn " : "", entity.interactable ? "use " : "",
                                entity.animated ? "anim " : "", entity.animator ? "animator " : "",
                                entity.hidden ? "hidden" : "");
                });
                ImGui::EndTable();
            }
        }
        ImGui::End();
    }
}
