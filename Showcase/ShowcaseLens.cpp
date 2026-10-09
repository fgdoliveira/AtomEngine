// The lens (v0.0.14): the developer's view of the Showcase. Tab pins a
// callout on each feature in view - what it is, its live cost - and lists
// every feature with its source file and manual section; 1-9 and 0 switch a
// feature off and back, to see what it contributes. The features are the
// catalog's (Features/Features.cpp); this file binds each to its switch and
// its numbers.
#include "ShowcaseApp.h"

#include "Platform/Input.h"
#include "Renderer/Renderer.h"
#include "UI/UIRenderer.h"

#include <SDL3/SDL.h>

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cstdio>

namespace Showcase
{
    namespace
    {
        constexpr glm::vec4 PanelColor{ 0.05f, 0.07f, 0.12f, 0.84f };
        constexpr glm::vec4 TitleColor{ 0.85f, 0.62f, 0.25f, 1.0f };   // a warm amber: the lens's own colour
        constexpr glm::vec4 TextColor{ 0.92f, 0.93f, 0.95f, 1.0f };
        constexpr glm::vec4 DimColor{ 0.62f, 0.66f, 0.74f, 1.0f };
        constexpr glm::vec4 OffColor{ 0.85f, 0.35f, 0.30f, 1.0f };
        constexpr float CalloutRange = 45.0f; // metres: farther features are listed, not pinned
    }

    int ShowcaseApp::FeatureIndex(std::string_view id)
    {
        const std::vector<FeatureInfo>& features = FeatureCatalog();
        for (std::size_t i = 0; i < features.size(); ++i)
        {
            if (id == features[i].id)
            {
                return static_cast<int>(i);
            }
        }
        return -1;
    }

    void ShowcaseApp::SetFeature(int index, bool on)
    {
        if (index < 0 || index >= static_cast<int>(FeatureCatalog().size()))
        {
            return;
        }
        m_featureOn[index] = on;
        Atom::Renderer& renderer = GetRenderer();
        Level* level = m_levels ? m_levels->GetLevel() : nullptr;
        const std::string_view id = FeatureCatalog()[index].id;
        if (id == "water") renderer.SetWaterEnabled(on);
        else if (id == "reflection") renderer.SetReflectionEnabled(on);
        else if (id == "shadows") m_view.shadows = on;
        else if (id == "fog") m_view.fogPreset = on ? DefaultFogPreset : 3; // 3: "off"
        else if (id == "weather") m_atmosphere.SetEnabled(on);
        else if (id == "lights" && level) level->SetLiveLightsEnabled(on);
        else if (id == "glow")
        {
            m_view.glow = on;
            if (level)
            {
                level->SetHalosEnabled(on);
            }
        }
        else if (id == "toon" && level) level->SetStylesEnabled(on);
        else if (id == "character")
        {
            // Off: the bind pose, no clips - skinning with nothing to show.
            if (Entity* rudy = FindEntity("rudy"); rudy && rudy->animator && m_mode == Mode::Walking)
            {
                rudy->animator->SetEnabled(on);
                if (!on)
                {
                    rudy->poseSamples.clear();
                }
            }
        }
        else if (id == "synth" && m_audio && m_audio->IsRunning())
        {
            m_audio->Send({ static_cast<int>(BoothCommand::Mute), on ? 0 : 1, 0.0f });
        }
        ApplyLighting();
    }

    void ShowcaseApp::ApplyFeatures()
    {
        for (int i = 0; i < static_cast<int>(FeatureCatalog().size()); ++i)
        {
            SetFeature(i, m_featureOn[i]);
        }
    }

    void ShowcaseApp::UpdateLens()
    {
        const Atom::Input& input = GetInput();
        if (input.WasKeyPressed(SDL_SCANCODE_TAB))
        {
            m_lens = !m_lens;
        }
        if (!m_lens)
        {
            return;
        }
        // 1-9 and 0: the features in catalog order.
        constexpr SDL_Scancode keys[10] = { SDL_SCANCODE_1, SDL_SCANCODE_2, SDL_SCANCODE_3, SDL_SCANCODE_4,
                                            SDL_SCANCODE_5, SDL_SCANCODE_6, SDL_SCANCODE_7, SDL_SCANCODE_8,
                                            SDL_SCANCODE_9, SDL_SCANCODE_0 };
        for (int i = 0; i < static_cast<int>(FeatureCatalog().size()); ++i)
        {
            if (input.WasKeyPressed(keys[i]))
            {
                SetFeature(i, !m_featureOn[i]);
                m_lensFocus = i;
            }
        }
    }

    std::string ShowcaseApp::FeatureCost(int index) const
    {
        if (!m_featureOn[index])
        {
            return "off";
        }
        Atom::Renderer& renderer = const_cast<ShowcaseApp*>(this)->GetRenderer();
        const Atom::FrameStats& stats = renderer.GetLastFrameStats();
        const Atom::SceneLighting& lighting = renderer.GetLighting();
        const Level* level = m_levels ? m_levels->GetLevel() : nullptr;
        const std::string_view id = FeatureCatalog()[index].id;
        char text[96];
        if (id == "water") std::snprintf(text, sizeof(text), "%u surfaces drawn", stats.waterDraws);
        else if (id == "reflection") std::snprintf(text, sizeof(text), "%u draws mirrored", stats.reflectionDrawn);
        else if (id == "shadows") std::snprintf(text, sizeof(text), "%u draws in the shadow pass", stats.shadowDrawn);
        else if (id == "fog") std::snprintf(text, sizeof(text), "density %.4f, every scene pixel", lighting.fogDensity);
        else if (id == "weather") std::snprintf(text, sizeof(text), "%u particles, %s", stats.particles, EnvironmentName().c_str());
        else if (id == "lights") std::snprintf(text, sizeof(text), "%u lights, %u draws lit", stats.liveLights, stats.liveLitDraws);
        else if (id == "glow")
        {
            std::snprintf(text, sizeof(text), "glow %.2f, %zu halos", lighting.glowStrength,
                          level ? level->GetData().halos.size() : std::size_t{ 0 });
        }
        else if (id == "toon")
        {
            std::size_t styled = 0;
            if (level)
            {
                for (const EntityData& entity : level->GetData().entities)
                {
                    styled += entity.style ? 1 : 0;
                }
            }
            std::snprintf(text, sizeof(text), "%zu styled models, own copies", styled);
        }
        else if (id == "character") std::snprintf(text, sizeof(text), "state: %s", AnimatorState("rudy").c_str());
        else if (id == "synth") std::snprintf(text, sizeof(text), "%s", m_audio && m_audio->IsRunning() ? "audio thread running" : "silent until a key");
        else text[0] = '\0';
        return text;
    }

    std::optional<glm::vec2> ShowcaseApp::Project(const glm::vec3& world) const
    {
        Atom::UIRenderer& ui = const_cast<ShowcaseApp*>(this)->GetRenderer().GetUI();
        const glm::vec2 screen = ui.GetScreenSize();
        const glm::mat4 viewProjection =
            glm::perspective(m_camera.verticalFov, screen.x / screen.y, m_camera.nearPlane, m_camera.farPlane)
            * m_camera.GetViewMatrix();
        const glm::vec4 clip = viewProjection * glm::vec4{ world, 1.0f };
        if (clip.w <= 0.01f)
        {
            return std::nullopt; // behind the camera
        }
        const glm::vec2 ndc{ clip.x / clip.w, clip.y / clip.w };
        if (std::abs(ndc.x) > 0.95f || std::abs(ndc.y) > 0.95f)
        {
            return std::nullopt; // off screen
        }
        return glm::vec2{ (ndc.x * 0.5f + 0.5f) * screen.x, (0.5f - ndc.y * 0.5f) * screen.y };
    }

    void ShowcaseApp::DrawLens()
    {
        Atom::UIRenderer& ui = GetRenderer().GetUI();
        const glm::vec2 screen = ui.GetScreenSize();
        const float scale = std::clamp(screen.y / 720.0f, 0.75f, 2.0f);
        const float pad = 8.0f * scale;
        const std::vector<FeatureInfo>& features = FeatureCatalog();
        const glm::vec3 eye = m_camera.GetPosition();

        // Callouts: on each feature in view, nearer than CalloutRange.
        for (int i = 0; i < static_cast<int>(features.size()); ++i)
        {
            const FeatureInfo& f = features[i];
            if (!f.anchored)
            {
                continue;
            }
            glm::vec3 at = f.point;
            if (*f.entity)
            {
                const Entity* entity = FindEntity(f.entity);
                if (!entity)
                {
                    continue;
                }
                at = entity->position + glm::vec3{ 0.0f, 2.2f, 0.0f };
            }
            if (glm::length(at - eye) > CalloutRange)
            {
                continue;
            }
            const std::optional<glm::vec2> p = Project(at);
            if (!p)
            {
                continue;
            }
            char head[96];
            std::snprintf(head, sizeof(head), "[%d] %s", (i + 1) % 10, f.title);
            const std::string cost = FeatureCost(i);
            const glm::vec2 a = ui.MeasureText(*m_smallFont, head, scale);
            const glm::vec2 b = ui.MeasureText(*m_smallFont, cost, scale);
            const glm::vec2 size{ std::max(a.x, b.x) + 2.0f * pad, a.y + b.y + 1.5f * pad };
            const glm::vec2 origin = *p - glm::vec2{ size.x * 0.5f, size.y + 10.0f * scale };
            ui.DrawRect(*p - glm::vec2{ 1.0f * scale, 10.0f * scale }, { 2.0f * scale, 10.0f * scale }, TitleColor);
            ui.DrawRect(origin, size, PanelColor);
            ui.DrawRect(origin, { 3.0f * scale, size.y }, m_featureOn[i] ? TitleColor : OffColor);
            ui.DrawText(*m_smallFont, head, origin + glm::vec2{ pad, pad * 0.5f }, TextColor, scale);
            ui.DrawText(*m_smallFont, cost, origin + glm::vec2{ pad, pad * 0.5f + a.y }, m_featureOn[i] ? DimColor : OffColor, scale);
        }

        // The panel: every feature, its switch and cost; the focused one's
        // engine system, source and manual section.
        // Three columns (the font is proportional: spaces can't align them).
        char line[200];
        std::snprintf(line, sizeof(line), "%.2f ms  (%.0f fps)   %s", m_smoothedMs,
                      m_smoothedMs > 0.0f ? 1000.0f / m_smoothedMs : 0.0f, EnvironmentName().c_str());
        const std::string frame = line;
        std::vector<std::string> keys, titles, costs;
        float keyWidth = 0.0f, titleWidth = 0.0f, costWidth = 0.0f;
        for (int i = 0; i < static_cast<int>(features.size()); ++i)
        {
            std::snprintf(line, sizeof(line), "[%d] %s", (i + 1) % 10, m_featureOn[i] ? "on" : "OFF");
            keys.push_back(line);
            titles.push_back(features[i].title);
            costs.push_back(FeatureCost(i));
            keyWidth = std::max(keyWidth, ui.MeasureText(*m_smallFont, keys.back(), scale).x);
            titleWidth = std::max(titleWidth, ui.MeasureText(*m_smallFont, titles.back(), scale).x);
            costWidth = std::max(costWidth, ui.MeasureText(*m_smallFont, costs.back(), scale).x);
        }
        const float gap = 12.0f * scale;
        const FeatureInfo& focus = features[m_lensFocus];
        const std::string system = focus.system;
        std::snprintf(line, sizeof(line), "%s   (manual section %d)", focus.source, focus.manual);
        const std::string where = line;

        const float x = screen.x - 16.0f * scale;
        float width = keyWidth + titleWidth + costWidth + 2.0f * gap;
        width = std::max({ width, ui.MeasureText(*m_smallFont, system, scale).x,
                           ui.MeasureText(*m_smallFont, where, scale).x, ui.MeasureText(*m_smallFont, frame, scale).x });
        const float lineHeight = m_smallFont->GetLineHeight() * scale;
        const float height = (features.size() + 4.5f) * lineHeight + 2.0f * pad;
        const glm::vec2 origin{ x - width - 2.0f * pad, 16.0f * scale };
        ui.DrawRect(origin, { width + 2.0f * pad, height }, PanelColor);
        glm::vec2 at = origin + glm::vec2{ pad };
        ui.DrawText(*m_smallFont, "LENS - how this place is made", at, TitleColor, scale);
        at.y += lineHeight * 1.2f;
        ui.DrawText(*m_smallFont, frame, at, TextColor, scale);
        at.y += lineHeight;
        for (std::size_t i = 0; i < features.size(); ++i)
        {
            const glm::vec4 color = m_featureOn[i] ? DimColor : OffColor;
            ui.DrawText(*m_smallFont, keys[i], at, color, scale);
            ui.DrawText(*m_smallFont, titles[i], at + glm::vec2{ keyWidth + gap, 0.0f },
                        static_cast<int>(i) == m_lensFocus ? TextColor : color, scale);
            ui.DrawText(*m_smallFont, costs[i], at + glm::vec2{ keyWidth + titleWidth + 2.0f * gap, 0.0f }, color, scale);
            at.y += lineHeight;
        }
        at.y += lineHeight * 0.3f;
        ui.DrawText(*m_smallFont, system, at, TextColor, scale);
        at.y += lineHeight;
        ui.DrawText(*m_smallFont, where, at, DimColor, scale);

        // The help line along the bottom.
        const char* help = "Tab close the lens    1-9, 0 switch a feature off and on    F1 numbers    F10 tools";
        const glm::vec2 helpSize = ui.MeasureText(*m_smallFont, help, scale);
        const glm::vec2 helpAt{ (screen.x - helpSize.x) * 0.5f, screen.y - helpSize.y - 24.0f * scale };
        ui.DrawRect(helpAt - glm::vec2{ pad, pad * 0.5f }, helpSize + glm::vec2{ 2.0f * pad, pad }, PanelColor);
        ui.DrawText(*m_smallFont, help, helpAt, DimColor, scale);
    }

    std::optional<float> ShowcaseApp::Stat(const std::string& name) const
    {
        Atom::Renderer& renderer = const_cast<ShowcaseApp*>(this)->GetRenderer();
        const Atom::FrameStats& stats = renderer.GetLastFrameStats();
        if (name == "devtools_panels") return static_cast<float>(m_devPanels.PanelsDrawn());
        if (name == "water_draws") return static_cast<float>(stats.waterDraws);
        if (name == "reflection_draws") return static_cast<float>(stats.reflectionDrawn);
        if (name == "shadow_draws") return static_cast<float>(stats.shadowDrawn);
        if (name == "particles") return static_cast<float>(stats.particles);
        if (name == "live_lights") return static_cast<float>(stats.liveLights);
        if (name == "glow_strength") return renderer.GetLighting().glowStrength;
        if (name == "fog_density") return renderer.GetLighting().fogDensity;
        if (name == "lens") return m_lens ? 1.0f : 0.0f;
        if (name.rfind("feature_", 0) == 0)
        {
            const int index = FeatureIndex(std::string_view(name).substr(8));
            return index < 0 ? std::nullopt : std::optional<float>(m_featureOn[index] ? 1.0f : 0.0f);
        }
        return std::nullopt;
    }
}
