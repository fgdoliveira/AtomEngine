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
        // Colours, panels and layout are the framework's UI kit (M89).
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
        UiKit kit(GetRenderer().GetUI(), *m_font, *m_smallFont);
        const UiTheme& theme = kit.Theme();
        const float pad = kit.Pad();
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
            if (const std::optional<glm::vec2> p = Project(at))
            {
                char head[96];
                std::snprintf(head, sizeof(head), "[%d] %s", (i + 1) % 10, f.title);
                kit.Callout(*p, head, FeatureCost(i), m_featureOn[i]);
            }
        }

        // The panel, top right: every feature, its switch and cost, in
        // measured columns; the focused one's engine system, source and
        // manual section.
        char line[200];
        std::snprintf(line, sizeof(line), "%.2f ms  (%.0f fps)   %s", m_smoothedMs,
                      m_smoothedMs > 0.0f ? 1000.0f / m_smoothedMs : 0.0f, EnvironmentName().c_str());
        const std::string frame = line;
        std::vector<std::vector<std::string>> rows;
        std::vector<glm::vec4> colors;
        for (int i = 0; i < static_cast<int>(features.size()); ++i)
        {
            std::snprintf(line, sizeof(line), "[%d] %s", (i + 1) % 10, m_featureOn[i] ? "on" : "OFF");
            rows.push_back({ line, features[i].title, FeatureCost(i) });
            colors.push_back(!m_featureOn[i] ? theme.warn : i == m_lensFocus ? theme.ink : theme.dim);
        }
        const FeatureInfo& focus = features[m_lensFocus];
        const std::string system = focus.system;
        std::snprintf(line, sizeof(line), "%s   (manual section %d)", focus.source, focus.manual);
        const std::string where = line;

        const float lineHeight = kit.LineHeight();
        const float width = std::max({ kit.ColumnsWidth(rows), kit.Measure(system).x, kit.Measure(where).x,
                                       kit.Measure(frame).x });
        const float height = (features.size() + 4.5f) * lineHeight + 2.0f * pad;
        const glm::vec2 origin{ kit.Screen().x - kit.Margin() - width - 3.0f * pad, kit.Margin() };
        kit.Panel(origin, { width + 3.0f * pad, height }, &theme.accent);
        glm::vec2 at = origin + glm::vec2{ 2.0f * pad, pad };
        kit.Text("LENS - how this place is made", at, theme.accent);
        at.y += lineHeight * 1.2f;
        kit.Text(frame, at, theme.ink);
        at.y += lineHeight;
        at.y += kit.Columns(at, rows, colors) + lineHeight * 0.3f;
        kit.Text(system, at, theme.ink);
        at.y += lineHeight;
        kit.Text(where, at, theme.dim);

        kit.HintBar("Tab close the lens    1-9, 0 switch a feature off and on    F1 numbers    F10 tools");
    }

    std::optional<float> ShowcaseApp::Stat(const std::string& name) const
    {
        Atom::Renderer& renderer = const_cast<ShowcaseApp*>(this)->GetRenderer();
        const Atom::FrameStats& stats = renderer.GetLastFrameStats();
        if (name == "menu") return m_screen == Screen::Menu ? 1.0f : 0.0f;
        if (name == "benchmark_running") return m_screen == Screen::Benchmark ? 1.0f : 0.0f;
        if (name == "benchmarks_done") return static_cast<float>(m_benchmarksDone);
        if (name == "benchmark_frames") return static_cast<float>(m_benchmarkFrames.Count());
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
