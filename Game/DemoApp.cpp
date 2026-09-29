#include "DemoApp.h"

#include "Interaction/ActionExecutor.h"
#include "Interaction/InteractionSystem.h"

#include <SDL3/SDL.h>

#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/mat4x4.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <filesystem>
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
        m_assetRoot = basePath ? basePath : "";
        // ATOM_ASSET_ROOT=<folder containing Assets/> reads the source tree
        // instead of the build's copy, and turns on hot reload.
        if (const char* root = SDL_getenv("ATOM_ASSET_ROOT"); root && *root)
        {
            m_assetRoot = root;
            if (m_assetRoot.back() != '/' && m_assetRoot.back() != '\\')
            {
                m_assetRoot += '/';
            }
            m_hotReload = true;
            std::cout << "Hot reload on: assets from " << m_assetRoot << '\n';
        }

        m_fogPreset = DefaultFogPreset;
        m_audioScape.Initialize(GetAudio());

        if (!m_atmosphere.Initialize(GetRenderer()) || !m_unease.Initialize(GetRenderer()))
        {
            return false;
        }

        const std::string fontPath = m_assetRoot + "Assets/Fonts/ShipporiMincho-Medium-Latin.ttf";
        m_font = Atom::Font::Load(GetRenderer(), fontPath, 30.0f);
        m_smallFont = Atom::Font::Load(GetRenderer(), fontPath, 19.0f);
        if (!m_font || !m_smallFont)
        {
            return false;
        }

        m_dialogues.LoadDirectory(m_assetRoot + "Assets/Dialogue");

        // Levels get the persistent services they need; the manager tells
        // us when one goes away and when the next one is ready.
        m_levels = std::make_unique<LevelManager>(Level::Services{
            GetRenderer(), GetAudio(), m_audioScape, m_assetRoot, m_modelCache });
        m_levels->onUnloading = [this](Level& outgoing) { OnLevelUnloading(outgoing); };
        m_levels->onLoaded = [this](Level& incoming, const SpawnPoint& spawn) {
            OnLevelLoaded(incoming, spawn);
        };
        m_levels->onReloaded = [this](Level& incoming) { OnLevelReloaded(incoming); };

        // ATOM_START_LEVEL=<name>[:<spawn>] starts somewhere else (testing).
        std::string startLevel = "street";
        std::string startSpawn;
        if (const char* start = SDL_getenv("ATOM_START_LEVEL"))
        {
            const std::string value = start;
            const std::size_t colon = value.find(':');
            startLevel = value.substr(0, colon);
            startSpawn = colon == std::string::npos ? "" : value.substr(colon + 1);
        }
        if (!m_levels->Load(startLevel, startSpawn))
        {
            return false;
        }

        std::cout
            << "Controls: WASD move, Shift jog, mouse look, E interact, Esc release/quit\n"
            << "  F2 render scale  F3 baked light  F4 MSAA  F5 fog  F6 shadows  F7 post look\n"
            << "  F8 particles  F9 unease events  M mute\n";

        LoadTestScript();

        // A scripted run doesn't need the mouse (and may not have focus).
        const bool captured = GetInput().SetMouseCaptured(GetWindow().GetSDLWindow(), true);
        return captured || m_testRunner != nullptr;
    }

    void DemoApp::OnLevelUnloading(Level& /*outgoing*/)
    {
        // Drop everything that points into the level that's about to die.
        m_target = {};
        m_speaker = {};
        m_sequence.Stop(); // its steps name the old level's entities
        if (m_dialogue.IsActive())
        {
            m_dialogue.Close();
        }
        m_unease.Configure({}, nullptr);
        m_audioScape.SetSurfaceProvider(nullptr);
    }

    void DemoApp::OnLevelLoaded(Level& incoming, const SpawnPoint& spawn)
    {
        const LevelData& data = incoming.GetData();

        m_player.Teleport(spawn.position, m_camera);
        m_camera.SetRotation(glm::radians(spawn.yawDegrees), 0.0f);
        // Expected from the spawn itself, not read back from the camera.
        m_arrivalEye = spawn.position + glm::vec3{ 0.0f, m_player.eyeHeight, 0.0f };
        m_arrivalYaw = glm::radians(spawn.yawDegrees);
        m_arriving = true;

        ConfigureForLevel(incoming);
        m_mode = Mode::Exploring;
        std::cout << "Entered level '" << data.name << "'\n";
    }

    void DemoApp::OnLevelReloaded(Level& incoming)
    {
        // Same place, same view: only the level's content changed.
        ConfigureForLevel(incoming);
        m_mode = Mode::Exploring;
    }

    void DemoApp::ConfigureForLevel(Level& incoming)
    {
        const LevelData& data = incoming.GetData();
        m_atmosphere.Configure(data.leaves, data.fogBanks);
        m_unease.Configure(data.unease, &incoming);
        m_audioScape.SetOutdoor(data.outdoor);
        m_audioScape.SetSurfaceProvider([&incoming](float x, float z) {
            return incoming.GetData().SurfaceAt(x, z);
        });
        ApplyLighting();
        WatchLevelFiles();
    }

    void DemoApp::WatchLevelFiles()
    {
        if (!m_hotReload)
        {
            return;
        }
        m_levelFiles.Watch(m_levels->GetSourceFiles());
        std::vector<std::string> dialogues;
        std::error_code error;
        for (const auto& entry : std::filesystem::directory_iterator(m_assetRoot + "Assets/Dialogue", error))
        {
            if (entry.path().extension() == ".json")
            {
                dialogues.push_back(entry.path().string());
            }
        }
        m_dialogueFiles.Watch(std::move(dialogues));
    }

    void DemoApp::UpdateHotReload(float deltaSeconds)
    {
        // Once a second, and only while walking: never mid-dialogue or
        // mid-transition.
        m_reloadTimer += deltaSeconds;
        if (!m_hotReload || m_reloadTimer < 1.0f || m_mode != Mode::Exploring)
        {
            return;
        }
        m_reloadTimer = 0.0f;

        if (!m_dialogueFiles.Poll().empty())
        {
            m_dialogues.LoadDirectory(m_assetRoot + "Assets/Dialogue");
            m_messages.Show("Dialogue reloaded");
        }
        const std::vector<std::string> changed = m_levelFiles.Poll();
        if (changed.empty())
        {
            return;
        }
        std::cout << "Changed: " << changed.front() << (changed.size() > 1 ? " (and more)" : "") << '\n';
        if (const std::string error = ReloadLevel(); !error.empty())
        {
            // Keep playing the old level; say what's wrong where you look.
            std::cerr << "Reload failed: " << error << '\n';
            m_messages.Show("Reload failed - " + error);
        }
        else
        {
            m_messages.Show("Level reloaded");
        }
    }

    std::string DemoApp::ReloadLevel()
    {
        const std::string error = m_levels->Reload();
        if (!error.empty())
        {
            // Don't retry the same broken files every second: wait for the
            // next edit.
            m_levelFiles.Poll();
        }
        return error;
    }

    GameWorld* DemoApp::CurrentWorld()
    {
        Level* level = m_levels ? m_levels->GetLevel() : nullptr;
        return level ? &level->GetWorld() : nullptr;
    }

    const Atom::CollisionWorld* DemoApp::CurrentCollision() const
    {
        const Level* level = m_levels ? m_levels->GetLevel() : nullptr;
        return level ? &level->GetCollision() : nullptr;
    }

    void DemoApp::OnUpdate(float deltaSeconds)
    {
        UpdateMouseCapture();
        UpdateRenderSettings();
        UpdateTestScript(deltaSeconds);

        m_time += deltaSeconds;
        m_messages.Update(deltaSeconds);

        m_levels->Update(deltaSeconds);
        GetRenderer().SetFade(m_levels->GetFade());
        UpdateHotReload(deltaSeconds);
        if (Level* level = m_levels->GetLevel())
        {
            level->Update(deltaSeconds, m_player.GetFeetPosition());
        }
        if (m_levels->IsTransitioning())
        {
            m_mode = Mode::Transitioning;
        }
        else if (m_mode == Mode::Transitioning)
        {
            m_mode = Mode::Exploring;
        }
        if (m_mode != Mode::Transitioning)
        {
            m_arriving = false; // the player may move from here on
        }

        switch (m_mode)
        {
        case Mode::Exploring:
            m_player.Update(GetInput(), m_camera, CurrentCollision(), deltaSeconds);
            UpdateInteraction();
            break;
        case Mode::InDialogue:
            UpdateDialogue(deltaSeconds);
            break;
        case Mode::InSequence:
            m_target = {};
            UpdateSequence(deltaSeconds);
            break;
        case Mode::Transitioning:
            m_target = {};
            // The fade-in is drawn from here: it must be the spawn.
            SDL_assert(!m_arriving || (Arrival().distance < 0.01f && Arrival().yawDegrees < 0.5f));
            break;
        }

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

        if (Level* level = m_levels->GetLevel())
        {
            level->Submit(renderer, m_player.GetFeetPosition());
        }

        // Fog banks stay faintly visible with fog off: morning haze.
        const Atom::SceneLighting& lighting = renderer.GetLighting();
        m_atmosphere.Update(
            deltaSeconds,
            m_player.GetFeetPosition(),
            lighting.fogColor,
            lighting.fogDensity > 0.0f ? 1.0f : 0.35f
        );
        m_atmosphere.Submit(renderer);
        // Sway follows the gusts outdoors; indoors the air is still.
        const Level* current = m_levels->GetLevel();
        renderer.SetWind(current && current->GetData().outdoor ? m_atmosphere.GetWind() : glm::vec3{ 0.0f }, m_time);

        m_unease.Update(deltaSeconds, m_camera, m_player.GetFeetPosition(), m_audioScape);
        m_unease.Submit(renderer);

        DrawOverlay(deltaSeconds);
        UpdateWindowTitle(deltaSeconds);
    }

    void DemoApp::OnShutdown()
    {
        GetRenderer().SetParticleAtlas(nullptr, 1);
        m_unease.Configure({}, nullptr);
        m_levels.reset(); // the current level cleans itself up
        m_smallFont.reset();
        m_font.reset();
        m_unease.Shutdown();
        m_atmosphere.Shutdown();
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
            "AtomEngine " ATOM_VERSION " | %s | %.0f fps | scene %ux%u %.0f%% MSAA %ux | fog %s"
            " | shadows %s | post %s | draws %u/%u (+%u) | particles %u"
            " | pos %.1f %.2f %.1f%s",
            m_levels->GetLevel() ? m_levels->GetLevel()->GetName().c_str() : "-",
            m_titleFrames / m_titleTimer,
            stats.sceneWidth,
            stats.sceneHeight,
            GetRenderer().GetSettings().renderScale * 100.0f,
            stats.msaaSamples,
            FogPresets[m_fogPreset].name,
            GetRenderer().GetLighting().shadowsEnabled ? "on" : m_shadowsEnabled ? "off (level)" : "off",
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

        if (m_mode == Mode::InDialogue)
        {
            m_dialogueView.Draw(ui, *m_font, *m_smallFont, m_dialogue, scale, m_time);
        }
        else if (m_mode == Mode::Exploring)
        {
            DrawInteractionPrompt(scale);
            if (hintAlpha <= 0.0f)
            {
                m_messages.Draw(ui, *m_font, scale * 0.85f);
            }
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

        // Per distance layer (M22): chunks in view, draw calls, triangles,
        // and shadow-pass draws - mid and far should show none.
        char layers[320];
        const char* layerNames[] = { "Near", "Mid ", "Far " };
        int written = 0;
        for (std::size_t i = 0; i < Atom::RenderLayerCount; ++i)
        {
            const Atom::LayerStats& l = stats.layers[i];
            written += std::snprintf(layers + written, sizeof(layers) - written,
                "%s  chunks %u/%u  draws %u  tris %.1fk  shadow %u\n",
                layerNames[i], l.chunksVisible, l.chunks, l.drawn, l.triangles / 1000.0f, l.shadowDrawn);
        }

        char text[1024];
        std::snprintf(text, sizeof(text),
            "%.2f ms  (%.0f fps)\n"
            "Scene %ux%u  (%.0f%%)  MSAA %ux\n"
            "Draws %u / %u   shadow casters %u\n"
            "%s"
            "Binds: pipelines %u  materials %u   Models loaded %zu, shared %zu\n"
            "Particles %u\n"
            "Fog %s   Shadows %s   Baked light %s   Post %s\n"
            "Particles %s   Unease %s   Audio %s\n"
            "Position %.1f  %.2f  %.1f\n"
            "Level %s   voices %zu   flags %zu",
            m_smoothedFrameMs,
            m_smoothedFrameMs > 0.0f ? 1000.0f / m_smoothedFrameMs : 0.0f,
            stats.sceneWidth, stats.sceneHeight, settings.renderScale * 100.0f, stats.msaaSamples,
            stats.drawn, stats.submitted, stats.shadowDrawn,
            layers,
            stats.pipelineBinds, stats.materialBinds, m_modelCache.GetLoads(), m_modelCache.GetHits(),
            stats.particles,
            FogPresets[m_fogPreset].name,
            GetRenderer().GetLighting().shadowsEnabled ? "on" : m_shadowsEnabled ? "off (level)" : "off",
            m_bakedLightEnabled ? "on" : "off",
            m_postMode == 0 ? "full" : m_postMode == 1 ? "grade" : "off",
            m_atmosphere.IsEnabled() ? "on" : "off",
            m_unease.IsEnabled() ? "on" : "off",
            m_audioScape.IsMuted() ? "muted" : "on",
            feet.x, feet.y, feet.z,
            m_levels->GetLevel() ? m_levels->GetLevel()->GetName().c_str() : "-",
            GetAudio().GetVoiceCount(),
            m_gameState.FlagCount());

        const float padding = 10.0f * scale;
        const glm::vec2 size = ui.MeasureText(*m_smallFont, text, scale);
        ui.DrawRect({ 12.0f * scale, 12.0f * scale }, size + glm::vec2{ 2.0f * padding },
            { 0.04f, 0.04f, 0.05f, 0.85f });
        ui.DrawText(*m_smallFont, text,
            glm::vec2{ 12.0f * scale + padding }, { 0.88f, 0.90f, 0.86f, 1.0f }, scale);
    }

    bool DemoApp::BeginDialogue(const std::string& dialogueId)
    {
        const Dialogue* dialogue = m_dialogues.Find(dialogueId);
        if (!dialogue)
        {
            return false;
        }
        m_dialogue.Start(*dialogue, m_gameState);
        m_speaker = m_target;
        m_mode = Mode::InDialogue;
        return true;
    }

    void DemoApp::UpdateDialogue(float deltaSeconds)
    {
        const Atom::Input& input = GetInput();
        m_dialogue.Update(deltaSeconds);

        if (input.WasKeyPressed(SDL_SCANCODE_W) || input.WasKeyPressed(SDL_SCANCODE_UP))
        {
            m_dialogue.MoveSelection(-1);
        }
        if (input.WasKeyPressed(SDL_SCANCODE_S) || input.WasKeyPressed(SDL_SCANCODE_DOWN))
        {
            m_dialogue.MoveSelection(1);
        }
        // Number keys pick a choice directly.
        for (int i = 0; i < 4; ++i)
        {
            if (input.WasKeyPressed(static_cast<SDL_Scancode>(SDL_SCANCODE_1 + i)))
            {
                m_dialogue.SelectIndex(i);
                m_dialogue.Confirm();
            }
        }
        if (input.WasKeyPressed(SDL_SCANCODE_E)
            || input.WasKeyPressed(SDL_SCANCODE_SPACE)
            || input.WasKeyPressed(SDL_SCANCODE_RETURN))
        {
            m_dialogue.Confirm();
        }

        // Face whoever is speaking.
        GameWorld* world = CurrentWorld();
        if (const Entity* speaker = world ? world->Find(m_speaker) : nullptr)
        {
            const glm::vec3 focus = speaker->position
                + (speaker->interactable ? speaker->interactable->focusOffset : glm::vec3{ 0.0f, 1.5f, 0.0f });
            TurnCameraToward(focus, deltaSeconds);
        }

        if (!m_dialogue.IsActive())
        {
            m_dialogue.Close();
            m_mode = Mode::Exploring;
            m_speaker = {};
        }
    }

    void DemoApp::TurnCameraToward(const glm::vec3& point, float deltaSeconds)
    {
        const glm::vec3 offset = point - m_camera.GetPosition();
        const float targetYaw = std::atan2(offset.x, -offset.z);
        const float targetPitch = std::atan2(offset.y, std::sqrt(offset.x * offset.x + offset.z * offset.z));

        // Shortest way round, eased: settles in about half a second.
        float yawDelta = std::remainder(targetYaw - m_camera.GetYaw(), glm::two_pi<float>());
        const float blend = 1.0f - std::exp(-8.0f * deltaSeconds);
        m_camera.SetRotation(
            m_camera.GetYaw() + yawDelta * blend,
            m_camera.GetPitch() + (targetPitch - m_camera.GetPitch()) * blend);
    }

    void DemoApp::UpdateInteraction()
    {
        const Atom::Input& input = GetInput();
        GameWorld* world = CurrentWorld();
        if ((!input.IsMouseCaptured() && !m_testRunner) || !world)
        {
            m_target = {};
            return;
        }

        m_target = InteractionSystem::FindTarget(
            *world, CurrentCollision(), m_camera.GetPosition(), m_camera.GetForward());

        const Entity* target = world->Find(m_target);
        if (target && input.WasKeyPressed(SDL_SCANCODE_E))
        {
            InteractWith(*target);
        }
    }

    void DemoApp::InteractWith(const Entity& target)
    {
        ActionContext context{
            m_gameState,
            m_messages,
            [this](const std::string& id) { return BeginDialogue(id); },
            [this](const std::string& level, const std::string& spawn) {
                m_levels->RequestChange(level, spawn);
            },
            [this](const std::string& entity, const std::string& clip) {
                Level* level = m_levels->GetLevel();
                return level && level->PlayAnimation(entity, clip);
            },
            [this](const std::string& id) { return RunSequence(id); },
        };
        std::cout << "Interacted with " << target.name << '\n';
        ExecuteAction(InteractionSystem::ResolveAction(*target.interactable, m_gameState), context);
    }

    bool DemoApp::RunSequence(const std::string& id)
    {
        Level* level = m_levels->GetLevel();
        if (!level || m_mode != Mode::Exploring)
        {
            return false;
        }
        const auto found = level->GetData().sequences.find(id);
        if (found == level->GetData().sequences.end() || !m_sequence.Start(found->second, id))
        {
            return false;
        }
        m_mode = Mode::InSequence;
        return true;
    }

    void DemoApp::UpdateSequence(float deltaSeconds)
    {
        Level* level = m_levels->GetLevel();
        if (!level)
        {
            m_sequence.Stop();
        }
        else
        {
            const SequenceHooks hooks{
                [this](const std::string& text) { m_messages.Show(text); },
                [this](const std::string& flag) { m_gameState.SetFlag(flag); std::cout << "Flag set: " << flag << '\n'; },
                [level](const std::string& entity, bool visible) { level->SetEntityVisible(entity, visible); },
                [this, level](const std::string& sound, const std::string& entity, float gain, bool loop) {
                    level->PlaySound(m_audioScape.GetSound(sound), entity, gain, loop);
                },
                [level](const std::string& entity, const std::string& clip) { return level->PlayAnimation(entity, clip); },
                [level](const std::string& entity) { return level->GetEntityPosition(entity); },
                [level](const std::string& entity, const glm::vec3& position) { level->SetEntityPosition(entity, position); },
                [this](const std::string& name, const std::string& spawn) { m_levels->RequestChange(name, spawn); },
            };
            m_sequence.Update(deltaSeconds, hooks);
        }
        // A sequence ending in a level change hands over to the transition.
        if (!m_sequence.IsRunning() && m_mode == Mode::InSequence)
        {
            m_mode = m_levels->IsTransitioning() ? Mode::Transitioning : Mode::Exploring;
        }
    }

    void DemoApp::DrawInteractionPrompt(float scale)
    {
        GameWorld* world = CurrentWorld();
        const Entity* target = world ? world->Find(m_target) : nullptr;
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

        // F3: baked light on/off, to compare with the flat hemisphere ambient.
        if (input.WasKeyPressed(SDL_SCANCODE_F3))
        {
            m_bakedLightEnabled = !m_bakedLightEnabled;
            ApplyLighting();
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
        // The level decides the light; the player's toggles (fog preset,
        // shadows) apply on top wherever they are.
        Atom::SceneLighting lighting{};
        if (const Level* level = m_levels ? m_levels->GetLevel() : nullptr)
        {
            const LevelLighting& l = level->GetData().lighting;
            lighting.sunDirection = l.sunDirection;
            lighting.sunColor = l.sunColor;
            lighting.skyColor = l.skyColor;
            lighting.groundColor = l.groundColor;
            lighting.fogColor = l.fogColor;
            lighting.shadowsEnabled = l.shadows && m_shadowsEnabled;
            lighting.bakedLight = m_bakedLightEnabled ? l.bakedLight : 0.0f;
            lighting.glowStrength = l.glowStrength;
            lighting.glowThreshold = l.glowThreshold;
            lighting.skyPanorama = level->GetSkyPanorama();
            lighting.skyIntensity = level->GetData().sky ? level->GetData().sky->intensity : 1.0f;
        }
        lighting.fogDensity = FogPresets[m_fogPreset].density;
        lighting.fogHeightFalloff = 0.08f;
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

    // --- Scripted tests ------------------------------------------------------

    void DemoApp::LoadTestScript()
    {
        const char* path = SDL_getenv("ATOM_TEST_SCRIPT");
        if (!path)
        {
            return;
        }

        size_t size = 0;
        void* text = SDL_LoadFile(path, &size);
        if (!text)
        {
            std::cerr << "[test] cannot read script '" << path << "'\n";
            RequestQuit(2);
            return;
        }
        TestScriptParseResult parsed = ParseTestScript(
            std::string_view(static_cast<const char*>(text), size));
        SDL_free(text);

        if (!parsed.error.empty())
        {
            std::cerr << "[test] invalid script: " << parsed.error << '\n';
            RequestQuit(2);
            return;
        }
        std::cout << "[test] running '" << path << "' (" << parsed.commands.size() << " commands)\n";
        m_testRunner = std::make_unique<TestRunner>(std::move(parsed.commands));
    }

    void DemoApp::UpdateTestScript(float deltaSeconds)
    {
        if (!m_testRunner || m_testRunner->IsFinished())
        {
            return;
        }
        m_testRunner->Update(deltaSeconds, *this);
        if (m_testRunner->IsFinished())
        {
            if (m_testRunner->Passed())
            {
                std::cout << "[test] PASS\n";
                RequestQuit(0);
            }
            else
            {
                std::cout << "[test] FAIL " << m_testRunner->GetFailure() << '\n';
                RequestQuit(1);
            }
        }
    }

    const Entity* DemoApp::FindEntity(const std::string& name)
    {
        GameWorld* world = CurrentWorld();
        const Entity* found = nullptr;
        if (world)
        {
            world->ForEach([&](EntityId, const Entity& entity) {
                if (entity.name == name)
                {
                    found = &entity;
                }
            });
        }
        return found;
    }

    bool DemoApp::TeleportTo(const std::string& name, float distance)
    {
        const Entity* entity = FindEntity(name);
        if (!entity)
        {
            return false;
        }
        const glm::vec3 focus = entity->position
            + (entity->interactable ? entity->interactable->focusOffset : glm::vec3{ 0.0f, 1.2f, 0.0f });

        // Stand `distance` away on the side the player is already on.
        glm::vec3 away = m_player.GetFeetPosition() - focus;
        away.y = 0.0f;
        const float length = glm::length(away);
        away = length > 0.01f ? away / length : glm::vec3{ 0.0f, 0.0f, 1.0f };

        glm::vec3 feet = focus + away * distance;
        float floor = 0.0f;
        if (const Atom::CollisionWorld* collision = CurrentCollision())
        {
            floor = collision->FindFloor({ feet.x, focus.y + 3.0f, feet.z }, 20.0f).value_or(0.0f);
        }
        feet.y = floor;
        m_player.Teleport(feet, m_camera);
        return Face(name);
    }

    void DemoApp::Teleport(const glm::vec3& feet, float yawDegrees)
    {
        m_player.Teleport(feet, m_camera);
        m_camera.SetRotation(glm::radians(yawDegrees), 0.0f);
    }

    bool DemoApp::Face(const std::string& name)
    {
        const Entity* entity = FindEntity(name);
        if (!entity)
        {
            return false;
        }
        const glm::vec3 focus = entity->position
            + (entity->interactable ? entity->interactable->focusOffset : glm::vec3{ 0.0f, 1.2f, 0.0f });
        const glm::vec3 eye = m_player.GetFeetPosition() + glm::vec3{ 0.0f, m_player.eyeHeight, 0.0f };
        const glm::vec3 offset = focus - eye;
        m_camera.SetRotation(
            std::atan2(offset.x, -offset.z),
            std::atan2(offset.y, std::sqrt(offset.x * offset.x + offset.z * offset.z)));
        return true;
    }

    std::string DemoApp::CurrentTarget()
    {
        GameWorld* world = CurrentWorld();
        if (!world || m_mode != Mode::Exploring)
        {
            return {};
        }
        const EntityId id = InteractionSystem::FindTarget(
            *world, CurrentCollision(), m_camera.GetPosition(), m_camera.GetForward());
        const Entity* entity = world->Find(id);
        return entity ? entity->name : std::string{};
    }

    bool DemoApp::Interact()
    {
        GameWorld* world = CurrentWorld();
        if (!world || m_mode != Mode::Exploring)
        {
            return false;
        }
        m_target = InteractionSystem::FindTarget(
            *world, CurrentCollision(), m_camera.GetPosition(), m_camera.GetForward());
        const Entity* entity = world->Find(m_target);
        if (!entity)
        {
            return false;
        }
        InteractWith(*entity);
        return true;
    }

    bool DemoApp::Choose(int index)
    {
        if (m_mode != Mode::InDialogue)
        {
            return false;
        }
        if (m_dialogue.GetState() == DialogueRunner::State::Revealing)
        {
            m_dialogue.Advance(); // finish the line first, as a player would
        }
        if (index < 0 || index >= static_cast<int>(m_dialogue.GetVisibleChoices().size()))
        {
            return false;
        }
        m_dialogue.SelectIndex(index);
        m_dialogue.Confirm();
        return true;
    }

    void DemoApp::Advance()
    {
        if (m_mode == Mode::InDialogue)
        {
            m_dialogue.Confirm();
        }
    }

    bool DemoApp::HasFlag(const std::string& flag) const
    {
        return m_gameState.HasFlag(flag);
    }

    std::string DemoApp::LevelName() const
    {
        const Level* level = m_levels ? m_levels->GetLevel() : nullptr;
        return level ? level->GetName() : std::string{};
    }

    std::string DemoApp::ModeName() const
    {
        switch (m_mode)
        {
        case Mode::InDialogue: return "dialogue";
        case Mode::Transitioning: return "transitioning";
        case Mode::InSequence: return "sequence";
        default: return "exploring";
        }
    }

    std::string DemoApp::Message() const
    {
        return m_messages.GetText();
    }

    std::string DemoApp::DialogueNodeId() const
    {
        const AtomGame::DialogueNode* node = m_dialogue.GetNode();
        return node && m_dialogue.IsActive() ? node->id : std::string{};
    }

    std::size_t DemoApp::VoiceCount() const
    {
        // Leak checks count what plays until stopped; a cicada call or a
        // footstep in flight isn't a leak.
        return const_cast<DemoApp*>(this)->GetAudio().GetLoopingVoiceCount();
    }

    std::string DemoApp::SurfaceName() const
    {
        const Level* level = m_levels->GetLevel();
        const glm::vec3& feet = m_player.GetFeetPosition();
        return level ? std::string(level->GetData().SurfaceAt(feet.x, feet.z)) : "";
    }

    float DemoApp::ZoneLevel(const std::string& cell) const
    {
        const Level* level = m_levels->GetLevel();
        return level ? level->GetZoneLevel(cell) : 0.0f;
    }

    ArrivalError DemoApp::Arrival() const
    {
        const float yaw = std::remainder(m_camera.GetYaw() - m_arrivalYaw, glm::two_pi<float>());
        return ArrivalError{
            glm::length(m_camera.GetPosition() - m_arrivalEye),
            std::abs(glm::degrees(yaw)),
        };
    }

    std::optional<float> DemoApp::AnimationTime(const std::string& name) const
    {
        const Entity* entity = const_cast<DemoApp*>(this)->FindEntity(name);
        if (!entity || !entity->animated)
        {
            return std::nullopt;
        }
        return entity->animated->time;
    }

    bool DemoApp::AnimationPlaying(const std::string& name) const
    {
        const Entity* entity = const_cast<DemoApp*>(this)->FindEntity(name);
        return entity && entity->animated && entity->animated->playing;
    }

    void DemoApp::Log(const std::string& text)
    {
        std::cout << "[test] " << text << '\n';
    }
}
