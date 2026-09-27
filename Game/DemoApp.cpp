#include "DemoApp.h"

#include "Interaction/ActionExecutor.h"
#include "Interaction/InteractionSystem.h"

#include <SDL3/SDL.h>

#include <glm/gtc/constants.hpp>
#include <glm/mat4x4.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <iostream>
#include <iterator>
#include <string>

namespace AtomGame
{
    namespace
    {
        struct FogPreset
        {
            const char* name;
            float density; // ~3/density metres until fully fogged
        };

        constexpr FogPreset FogPresets[] = {
            { "dense", 0.13f },
            { "medium", 0.085f },
            { "light", 0.045f },
            { "off", 0.0f },
        };
        constexpr std::size_t DefaultFogPreset = 3; // off
    }

    bool DemoApp::OnInitialize()
    {
        const char* basePath = SDL_GetBasePath();
        const std::string streetPath =
            std::string(basePath ? basePath : "") + "Assets/Street/";

        m_street = Atom::Model::Load(GetRenderer(), streetPath + "street.glb");
        if (!m_street || !m_collision.Load(streetPath + "street_col.glb"))
        {
            return false;
        }

        m_fogPreset = DefaultFogPreset;
        ApplyLighting();

        m_audioScape.Initialize(GetAudio());

        if (!m_atmosphere.Initialize(GetRenderer()))
        {
            return false;
        }

        const std::string fontPath = std::string(basePath ? basePath : "")
            + "Assets/Fonts/ShipporiMincho-Medium-Latin.ttf";
        m_font = Atom::Font::Load(GetRenderer(), fontPath, 30.0f);
        m_smallFont = Atom::Font::Load(GetRenderer(), fontPath, 19.0f);
        if (!m_font || !m_smallFont)
        {
            return false;
        }

        // Blender material names survive into glTF ("atom_" + kit name).
        if (!m_unease.Initialize(
            GetRenderer(), m_street->FindMaterial("atom_vending_front")))
        {
            return false;
        }

        SpawnStreetEntities();

        std::cout
            << "Controls: WASD move, Shift jog, mouse look, E interact, Esc release/quit\n"
            << "  F2 render scale  F4 MSAA  F5 fog  F6 shadows  F7 post look\n"
            << "  F8 particles  F9 unease events  M mute\n";

        // East end of the street, looking west along it.
        m_player.SetFeetPosition(glm::vec3{ 36.0f, 0.0f, 1.0f });
        m_camera.SetRotation(-glm::half_pi<float>(), 0.0f);

        return GetInput().SetMouseCaptured(GetWindow().GetSDLWindow(), true);
    }

    void DemoApp::OnUpdate(float deltaSeconds)
    {
        UpdateMouseCapture();
        UpdateRenderSettings();

        m_player.Update(GetInput(), m_camera, &m_collision, deltaSeconds);
        UpdateInteraction(deltaSeconds);

        const Atom::Input& input = GetInput();
        if (input.WasKeyPressed(SDL_SCANCODE_M))
        {
            m_audioScape.ToggleMute();
        }
        m_audioScape.Update(deltaSeconds, m_camera, AudioScape::Listener{
            m_player.GetFeetPosition(),
            m_player.GetStepCount(),
            m_player.IsGrounded(),
            input.IsKeyDown(SDL_SCANCODE_LSHIFT)
        });

        Atom::Renderer& renderer = GetRenderer();
        renderer.SetCamera(
            m_camera.GetViewMatrix(),
            m_camera.verticalFov,
            m_camera.nearPlane,
            m_camera.farPlane
        );

        m_street->Submit(renderer, glm::mat4{ 1.0f });

        // Fog banks stay faintly visible with fog off: morning haze.
        const Atom::SceneLighting& lighting = renderer.GetLighting();
        m_atmosphere.Update(
            deltaSeconds,
            m_player.GetFeetPosition(),
            lighting.fogColor,
            lighting.fogDensity > 0.0f ? 1.0f : 0.35f
        );
        m_atmosphere.Submit(renderer);

        m_unease.Update(deltaSeconds, m_camera, m_player.GetFeetPosition(), m_audioScape);
        m_unease.Submit(renderer);

        DrawOverlay(deltaSeconds);
        UpdateWindowTitle(deltaSeconds);
    }

    void DemoApp::OnShutdown()
    {
        GetRenderer().SetParticleAtlas(nullptr, 1);
        m_smallFont.reset();
        m_font.reset();
        m_unease.Shutdown();
        m_atmosphere.Shutdown();
        m_street.reset();
    }

    void DemoApp::UpdateWindowTitle(float deltaSeconds)
    {
        m_titleTimer += deltaSeconds;
        ++m_titleFrames;
        if (m_titleTimer < 0.5f)
        {
            return;
        }

        // Stats lag one frame (they describe the last Render()).
        const Atom::FrameStats& stats = GetRenderer().GetLastFrameStats();
        const glm::vec3& feet = m_player.GetFeetPosition();

        char title[256];
        std::snprintf(
            title,
            sizeof(title),
            "AtomEngine | %.0f fps | scene %ux%u %.0f%% MSAA %ux | fog %s"
            " | shadows %s | post %s | draws %u/%u (+%u) | particles %u"
            " | pos %.1f %.2f %.1f%s",
            m_titleFrames / m_titleTimer,
            stats.sceneWidth,
            stats.sceneHeight,
            GetRenderer().GetSettings().renderScale * 100.0f,
            stats.msaaSamples,
            FogPresets[m_fogPreset].name,
            m_shadowsEnabled ? "on" : "off",
            m_postMode == 0 ? "full" : m_postMode == 1 ? "grade" : "off",
            stats.drawn,
            stats.submitted,
            stats.shadowDrawn,
            stats.particles,
            feet.x,
            feet.y,
            feet.z,
            m_unease.IsFigureVisible() ? " | figure" : ""
        );
        SDL_SetWindowTitle(GetWindow().GetSDLWindow(), title);

        m_titleTimer = 0.0f;
        m_titleFrames = 0;
    }

    void DemoApp::DrawOverlay(float deltaSeconds)
    {
        Atom::UIRenderer& ui = GetRenderer().GetUI();
        const glm::vec2 screen = ui.GetScreenSize();
        // Lay out for a 720-line screen and scale with the window.
        const float scale = std::clamp(screen.y / 720.0f, 0.75f, 2.0f);

        // Controls hint: shown on arrival, then fades away.
        m_hintTime += deltaSeconds;
        const float hintAlpha = std::clamp((9.0f - m_hintTime) / 1.5f, 0.0f, 1.0f);
        if (hintAlpha > 0.0f)
        {
            const char* hint = "WASD move   Shift jog   Mouse look   E interact   F1 debug";
            const glm::vec2 size = ui.MeasureText(*m_font, hint, scale * 0.8f);
            const glm::vec2 position{ (screen.x - size.x) * 0.5f, screen.y - size.y - 40.0f * scale };
            // A soft shadow keeps light text legible over the pale fog.
            ui.DrawText(*m_font, hint, position + glm::vec2{ 2.0f * scale },
                { 0.0f, 0.0f, 0.0f, 0.55f * hintAlpha }, scale * 0.8f);
            ui.DrawText(*m_font, hint, position,
                { 0.92f, 0.90f, 0.84f, hintAlpha }, scale * 0.8f);
        }

        DrawInteractionPrompt(scale);
        if (hintAlpha <= 0.0f)
        {
            m_messages.Draw(ui, *m_font, scale * 0.85f);
        }

        // Frame time, smoothed so the numbers are readable.
        const float frameMs = deltaSeconds * 1000.0f;
        m_smoothedFrameMs += (frameMs - m_smoothedFrameMs) * 0.05f;

        if (!m_showDebugOverlay)
        {
            return;
        }

        const Atom::FrameStats& stats = GetRenderer().GetLastFrameStats();
        const Atom::RenderSettings& settings = GetRenderer().GetSettings();
        const glm::vec3& feet = m_player.GetFeetPosition();

        char text[640];
        std::snprintf(text, sizeof(text),
            "%.2f ms  (%.0f fps)\n"
            "Scene %ux%u  (%.0f%%)  MSAA %ux\n"
            "Draws %u / %u   shadow casters %u\n"
            "Particles %u\n"
            "Fog %s   Shadows %s   Post %s\n"
            "Particles %s   Unease %s   Audio %s\n"
            "Position %.1f  %.2f  %.1f",
            m_smoothedFrameMs,
            m_smoothedFrameMs > 0.0f ? 1000.0f / m_smoothedFrameMs : 0.0f,
            stats.sceneWidth, stats.sceneHeight, settings.renderScale * 100.0f, stats.msaaSamples,
            stats.drawn, stats.submitted, stats.shadowDrawn,
            stats.particles,
            FogPresets[m_fogPreset].name,
            m_shadowsEnabled ? "on" : "off",
            m_postMode == 0 ? "full" : m_postMode == 1 ? "grade" : "off",
            m_atmosphere.IsEnabled() ? "on" : "off",
            m_unease.IsEnabled() ? "on" : "off",
            m_audioScape.IsMuted() ? "muted" : "on",
            feet.x, feet.y, feet.z);

        const float padding = 10.0f * scale;
        const glm::vec2 size = ui.MeasureText(*m_smallFont, text, scale);
        ui.DrawRect({ 12.0f * scale, 12.0f * scale }, size + glm::vec2{ 2.0f * padding },
            { 0.04f, 0.04f, 0.05f, 0.85f });
        ui.DrawText(*m_smallFont, text,
            glm::vec2{ 12.0f * scale + padding }, { 0.88f, 0.90f, 0.86f, 1.0f }, scale);
    }

    void DemoApp::SpawnStreetEntities()
    {
        // Placeholders in code until levels load entities from data (M12).
        // Positions are glTF space; the kit's fronts face +Z on the north
        // side of the road and -Z on the south side.
        const auto vending = [&](glm::vec3 position, float facing) {
            Entity entity;
            entity.name = "vending_machine";
            entity.position = position;
            Interactable use{ "Buy a drink", ShowMessage{
                "The coin drops. Something rattles inside... nothing comes out." } };
            use.focusOffset = { 0.0f, 1.1f, 0.45f * facing };
            entity.interactable = std::move(use);
            m_world.Spawn(std::move(entity));
        };
        vending({ -20.0f, 0.0f, -4.1f }, 1.0f);
        vending({ 29.8f, 0.0f, -4.1f }, 1.0f);
        vending({ -2.6f, 0.0f, 4.1f }, -1.0f);

        // Bow at the torii before approaching the shrine.
        Entity torii;
        torii.name = "torii";
        torii.position = { -14.0f, 0.0f, 5.5f };
        Interactable bow{ "Bow", SetFlag{ "bowed_at_torii",
            "You bow before passing beneath the torii." } };
        bow.focusOffset = { 0.0f, 1.6f, 0.0f };
        bow.radius = 2.5f;
        torii.interactable = std::move(bow);
        m_world.Spawn(std::move(torii));

        // The hokora answers only if you bowed first: a flag-gated action,
        // the same mechanism the shrine gate will use in M12.
        Entity hokora;
        hokora.name = "hokora";
        hokora.position = { -14.0f, 0.0f, 11.0f };
        Interactable pray{ "Pray", SetFlag{ "prayed_at_hokora",
            "You put your hands together. For a moment, the cicadas fall silent." } };
        pray.focusOffset = { 0.0f, 1.0f, -0.7f };
        pray.requiresFlag = "bowed_at_torii";
        pray.lockedAction = ShowMessage{
            "It feels wrong to come this close without bowing at the torii first." };
        hokora.interactable = std::move(pray);
        m_world.Spawn(std::move(hokora));

        std::cout << "Spawned " << m_world.Count() << " street entities\n";
    }

    void DemoApp::UpdateInteraction(float deltaSeconds)
    {
        m_messages.Update(deltaSeconds);

        const Atom::Input& input = GetInput();
        if (!input.IsMouseCaptured())
        {
            m_target = {};
            return;
        }

        m_target = InteractionSystem::FindTarget(
            m_world, &m_collision, m_camera.GetPosition(), m_camera.GetForward());

        const Entity* target = m_world.Find(m_target);
        if (target && input.WasKeyPressed(SDL_SCANCODE_E))
        {
            ActionContext context{ m_gameState, m_messages };
            ExecuteAction(
                InteractionSystem::ResolveAction(*target->interactable, m_gameState),
                context);
            std::cout << "Interacted with " << target->name << '\n';
        }
    }

    void DemoApp::DrawInteractionPrompt(float scale)
    {
        const Entity* target = m_world.Find(m_target);
        if (!target || m_messages.IsVisible())
        {
            return;
        }

        Atom::UIRenderer& ui = GetRenderer().GetUI();
        const glm::vec2 screen = ui.GetScreenSize();
        const std::string prompt = "[E]  " + target->interactable->prompt;
        const float textScale = scale * 0.9f;
        const glm::vec2 size = ui.MeasureText(*m_font, prompt, textScale);
        const glm::vec2 position{ (screen.x - size.x) * 0.5f, screen.y * 0.62f };

        ui.DrawText(*m_font, prompt, position + glm::vec2{ 2.0f * scale },
            { 0.0f, 0.0f, 0.0f, 0.6f }, textScale);
        ui.DrawText(*m_font, prompt, position, { 0.95f, 0.93f, 0.86f, 1.0f }, textScale);
    }

    void DemoApp::UpdateRenderSettings()
    {
        const Atom::Input& input = GetInput();
        Atom::Renderer& renderer = GetRenderer();
        Atom::RenderSettings settings = renderer.GetSettings();

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

        // F5: fog dense -> medium -> light -> off.
        if (input.WasKeyPressed(SDL_SCANCODE_F5))
        {
            m_fogPreset = (m_fogPreset + 1) % std::size(FogPresets);
            ApplyLighting();
        }

        // F7: post look full -> grade only (no grain/vignette) -> off.
        if (input.WasKeyPressed(SDL_SCANCODE_F7))
        {
            m_postMode = (m_postMode + 1) % 3;
            const Atom::PostSettings defaults{};
            settings.post = defaults;
            settings.post.enabled = m_postMode != 2;
            if (m_postMode == 1)
            {
                settings.post.grain = 0.0f;
                settings.post.vignette = 0.0f;
            }
            renderer.SetSettings(settings);
        }

        // F1: debug overlay.
        if (input.WasKeyPressed(SDL_SCANCODE_F1))
        {
            m_showDebugOverlay = !m_showDebugOverlay;
        }

        // F8: particles (leaves, ash, fog banks) on/off.
        if (input.WasKeyPressed(SDL_SCANCODE_F8))
        {
            m_atmosphere.SetEnabled(!m_atmosphere.IsEnabled());
        }

        // F9: unease events (figure, static, flicker) on/off.
        if (input.WasKeyPressed(SDL_SCANCODE_F9))
        {
            m_unease.SetEnabled(!m_unease.IsEnabled());
        }

        // F6: sun shadows on/off.
        if (input.WasKeyPressed(SDL_SCANCODE_F6))
        {
            m_shadowsEnabled = !m_shadowsEnabled;
            ApplyLighting();
        }

        // F4: MSAA 4x -> 2x -> 1x -> 4x.
        if (input.WasKeyPressed(SDL_SCANCODE_F4))
        {
            settings.msaaSamples = settings.msaaSamples > 1
                ? settings.msaaSamples / 2
                : 4;
            renderer.SetSettings(settings);
        }
    }

    void DemoApp::ApplyLighting()
    {
        // Overcast daylight; the sky is the fog colour.
        Atom::SceneLighting lighting{};
        lighting.fogColor = glm::vec3{ 0.46f, 0.47f, 0.47f };
        lighting.fogDensity = FogPresets[m_fogPreset].density;
        lighting.fogHeightFalloff = 0.08f;
        lighting.shadowsEnabled = m_shadowsEnabled;
        // Afternoon sun low in the north-east: the north-side houses throw
        // long, soft shadows across the road.
        lighting.sunDirection = glm::vec3{ 0.35f, 0.6f, -0.55f };
        GetRenderer().SetLighting(lighting);
    }

    void DemoApp::UpdateMouseCapture()
    {
        Atom::Input& input = GetInput();

        // Escape releases the mouse; clicking back in recaptures it.
        if (input.WasKeyPressed(SDL_SCANCODE_ESCAPE))
        {
            if (input.IsMouseCaptured())
            {
                input.SetMouseCaptured(GetWindow().GetSDLWindow(), false);
            }
            else
            {
                RequestQuit();
            }
        }
        else if (!input.IsMouseCaptured()
            && (SDL_GetMouseState(nullptr, nullptr) & SDL_BUTTON_LMASK))
        {
            input.SetMouseCaptured(GetWindow().GetSDLWindow(), true);
        }
    }
}
