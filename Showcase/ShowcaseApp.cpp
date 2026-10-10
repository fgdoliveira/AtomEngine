#include "ShowcaseApp.h"

#include "Character/LabViewer.h"
#include "Core/DevSwitch.h"
#include "Physics/CollisionWorld.h"
#include "Platform/Input.h"
#include "Platform/Window.h"
#include "Renderer/Renderer.h"
#include "UI/UIRenderer.h"

#include <SDL3/SDL.h>

#include <glm/gtc/constants.hpp>
#include <glm/geometric.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <iostream>

namespace Showcase
{
    const std::vector<ShowcaseApp::Area>& ShowcaseApp::Areas()
    {
        // In the order of the number keys. Places, not features: what the
        // engine does here is the lens's to say (Tab).
        static const std::vector<Area> areas = {
            { "village", "The village", "Houses along the lane; the lamps come on as night falls" },
            { "jetty", "The jetty", "The lake, and the sky in it" },
            { "workshop", "The workshop", "Someone is at work here - E to watch them closely" },
            { "shrine", "The shrine", "A small shrine under the cedars" },
            { "radio", "The radio shed", "An old radio: J K L ; play, U I tone, O echo" },
        };
        return areas;
    }

    bool ShowcaseApp::OnInitialize()
    {
        SDL_SetWindowTitle(GetWindow().GetSDLWindow(), "AtomEngine Showcase");
        RegisterDevPanels();

        // Beside the executable: the Showcase's own assets (Assets/Showcase)
        // over the shared ones (Assets/). ATOM_ASSET_ROOT=<the repository>
        // reads the source tree instead (hot reload's convention).
        const char* basePath = SDL_GetBasePath();
        m_outputRoot = basePath ? basePath : "";
        m_assets = AssetRoots({ m_outputRoot + "Assets/Showcase/", m_outputRoot + "Assets/" });
        if (const char* root = Atom::DevSwitch("ATOM_ASSET_ROOT"); root && *root)
        {
            m_outputRoot = root;
            if (m_outputRoot.back() != '/' && m_outputRoot.back() != '\\')
            {
                m_outputRoot += '/';
            }
            m_assets = AssetRoots({ m_outputRoot + "Showcase/Assets/", m_outputRoot + "Content/" });
            std::cout << "Assets from the source tree: " << m_outputRoot << '\n';
        }

        if (!m_atmosphere.Initialize(GetRenderer()))
        {
            return false;
        }
        const std::string fontPath = m_assets.Resolve("Fonts/ShipporiMincho-Medium-Latin.ttf");
        m_font = Atom::Font::Load(GetRenderer(), fontPath, 30.0f);
        m_smallFont = Atom::Font::Load(GetRenderer(), fontPath, 19.0f);
        if (!m_font || !m_smallFont)
        {
            return false;
        }
        for (const std::string& problem : m_presets.Load(m_assets.Resolve("Environments")))
        {
            std::cerr << "Environments/" << problem << '\n';
        }

        // No screens, no level sounds: the Showcase supplies neither.
        m_levels = std::make_unique<LevelManager>(Level::Services{
            GetRenderer(), GetAudio(), m_sounds, m_assets, m_modelCache, ScreenFactory{} });
        m_levels->onLoaded = [this](Level& incoming, const SpawnPoint& spawn) { OnLevelLoaded(incoming, spawn); };
        m_levels->onReloaded = [this](Level&) { ResetEnvironment(); ApplyLighting(); };

        // ATOM_START_LEVEL=<level>[:<spawn>] starts elsewhere: an area of the
        // scene (showcase:lights), or one of the documentation stages the
        // capture scripts film (first_render, character_lab).
        std::string startLevel = "showcase";
        std::string startSpawn;
        if (const char* start = Atom::DevSwitch("ATOM_START_LEVEL"))
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

        std::cout << "Showcase: 1-5 places, P next time of day, T pause time, Tab the lens, WASD/Shift/mouse walk,"
                     " F1 numbers, F2-F8 view, F10 tools, Esc menu\n";
        if (const std::optional<int> exitCode = m_diagnostics.InitializeFromEnvironment())
        {
            RequestQuit(*exitCode); // a script that can't run
        }
        // A scenario's time stands still unless it starts the clock ("set clock on").
        m_clockOn = !m_diagnostics.HasTestScript();
        ApplySettings();
        // M90: a player's run opens on the title, over the living village;
        // a scenario or a chosen start (ATOM_START_LEVEL) goes straight in.
        if (!m_diagnostics.HasTestScript() && !Atom::DevSwitch("ATOM_START_LEVEL"))
        {
            OpenMenu();
            return true;
        }
        const bool captured = GetInput().SetMouseCaptured(GetWindow().GetSDLWindow(), true);
        return captured || m_diagnostics.HasTestScript();
    }

    void ShowcaseApp::OnLevelLoaded(Level& incoming, const SpawnPoint& spawn)
    {
        m_player.Teleport(spawn.position, m_camera);
        m_camera.SetRotation(glm::radians(spawn.yawDegrees), 0.0f);
        m_arrivalEye = spawn.position + glm::vec3{ 0.0f, m_player.eyeHeight, 0.0f };
        m_arrivalYaw = glm::radians(spawn.yawDegrees);
        const LevelData& data = incoming.GetData();
        m_atmosphere.Configure(data.leaves, data.fogBanks, data.dust);
        ResetEnvironment();
        ApplyLighting();
        m_mode = Mode::Walking;
        ApplyFeatures(); // the lens's switches survive a (re)load
        std::cout << "Entered level '" << data.name << "'\n";
        // A documentation stage with a subject (character_lab) opens in the
        // viewer, as the demo's lab did; in the scene, E opens it.
        if (data.lab && data.name != "showcase")
        {
            BeginLab();
            m_arrivalEye = m_viewer.GetEye();
            m_arrivalYaw = m_viewer.GetCameraYaw();
        }
    }

    void ShowcaseApp::ResetEnvironment()
    {
        const Level* level = m_levels ? m_levels->GetLevel() : nullptr;
        m_environmentName.clear();
        if (level && level->GetData().environment && m_presets.Has(level->GetData().environment->defaultPreset))
        {
            m_environmentName = level->GetData().environment->defaultPreset;
        }
        m_environment.Reset(m_presets.Resolve(level, m_environmentName));
    }

    void ShowcaseApp::ApplyLighting()
    {
        const Level* level = m_levels ? m_levels->GetLevel() : nullptr;
        GetRenderer().SetLighting(SceneLightingFor(level, m_environment.Current(), m_view));
    }

    void ShowcaseApp::OnUpdate(float deltaSeconds)
    {
        const Level* measured = m_levels->GetLevel();
        m_diagnostics.RecordFrameTime(deltaSeconds, measured ? measured->GetName() : std::string{ "-" });
        deltaSeconds = m_diagnostics.Step(deltaSeconds);
        UpdateMouseCapture();
        if (HandleViewKeys(GetInput(), GetRenderer(), m_view, &m_atmosphere))
        {
            ApplyLighting();
        }
        if (const std::optional<int> exitCode = m_diagnostics.UpdateTestScript(deltaSeconds, *this))
        {
            RequestQuit(*exitCode);
        }
        m_time += deltaSeconds;
        // The lens's frame time: real, smoothed so it reads.
        m_smoothedMs += (static_cast<float>(m_diagnostics.RealFrameMs()) - m_smoothedMs) * 0.05f;
        UpdateScreens(deltaSeconds); // the menus and the benchmark (M90)

        m_levels->Update(deltaSeconds);
        GetRenderer().SetFade(m_levels->GetFade());
        UpdateClock(deltaSeconds);
        if (m_environment.IsTransitioning())
        {
            m_environment.Update(deltaSeconds);
            ApplyLighting();
        }
        if (m_mode == Mode::Walking)
        {
            UpdatePavilion();
        }
        // Where the world is seen from: the player, or the benchmark's camera.
        const glm::vec3 focus = m_screen == Screen::Benchmark ? m_camera.GetPosition() : m_player.GetFeetPosition();
        if (Level* level = m_levels->GetLevel())
        {
            level->Update(deltaSeconds, focus);
        }

        if (m_screen == Screen::None && !m_levels->IsTransitioning())
        {
            // E at the pavilion opens the character lab; in the lab, Tab
            // switches viewer and drive, E leaves.
            const bool lab = LabKeyPressed("lab", SDL_SCANCODE_E);
            const bool tab = LabKeyPressed("toggle_drive", SDL_SCANCODE_TAB);
            switch (m_mode)
            {
            case Mode::Walking:
            {
                // The key works at the pavilion; a scenario's "lab" anywhere.
                const Area* area = AreaAt(m_player.GetFeetPosition());
                const bool scripted = std::find(m_pressedActions.begin(), m_pressedActions.end(), "lab") != m_pressedActions.end();
                if (lab && (scripted || (area && std::string_view(area->spawn) == "workshop")))
                {
                    m_lens = false; // the lab has its own panel
                    BeginLab();
                    break;
                }
                UpdateLens();
                UpdateKeys();
                UpdateBooth();
                const Atom::Input& input = GetInput();
                const auto held = [&](SDL_Scancode a, SDL_Scancode b) { return input.IsKeyDown(a) || input.IsKeyDown(b); };
                PlayerController::MoveIntent intent;
                intent.move.y = (held(SDL_SCANCODE_W, SDL_SCANCODE_UP) ? 1.0f : 0.0f) - (held(SDL_SCANCODE_S, SDL_SCANCODE_DOWN) ? 1.0f : 0.0f);
                intent.move.x = (held(SDL_SCANCODE_D, SDL_SCANCODE_RIGHT) ? 1.0f : 0.0f) - (held(SDL_SCANCODE_A, SDL_SCANCODE_LEFT) ? 1.0f : 0.0f);
                intent.jog = input.IsKeyDown(SDL_SCANCODE_LSHIFT);
                m_player.Update(input, intent, m_camera, CurrentCollision(), deltaSeconds);
                break;
            }
            case Mode::Viewing:
                if (lab) { EndLab(); }
                else if (tab) { BeginDrive(); }
                else { UpdateLab(deltaSeconds); }
                break;
            case Mode::Driving:
                if (lab) { EndDrive(); EndLab(); }
                else if (tab) { EndDrive(); }
                else { UpdateDrive(deltaSeconds); }
                break;
            }
        }
        m_pressedActions.clear(); // a scenario's press lasts one frame, as a key's does

        Atom::Renderer& renderer = GetRenderer();
        renderer.SetCamera(m_camera.GetViewMatrix(), m_camera.verticalFov, m_camera.nearPlane, m_camera.farPlane);
        const Level* drawn = m_drawWorld ? m_levels->GetLevel() : nullptr;
        if (drawn)
        {
            drawn->Submit(renderer, focus);
        }

        // Weather (M50): the environment's wind and, outdoors, its rain.
        const Atom::SceneLighting& lighting = renderer.GetLighting();
        m_atmosphere.SetWind(m_environment.Current().wind);
        m_atmosphere.SetRain(lighting.rain, lighting.skyColor * 0.55f + glm::vec3{ 0.18f });
        m_atmosphere.Update(deltaSeconds, focus, lighting.fogColor,
                            lighting.fogDensity > 0.0f ? 1.0f : 0.35f);
        if (m_drawWorld)
        {
            m_atmosphere.Submit(renderer);
        }
        renderer.SetWind(m_atmosphere.GetWind(), m_time);

        if (m_screen != Screen::None)
        {
            DrawScreens();
        }
        else if (m_mode != Mode::Walking)
        {
            DrawLabOverlay(); // the skeleton always; the panel with the HUD
        }
        else if (m_lens)
        {
            DrawLens();
        }
        else if (m_showHud)
        {
            DrawCaption();
        }
        DrawDevTools(deltaSeconds);
    }

    void ShowcaseApp::UpdateKeys()
    {
        const Atom::Input& input = GetInput();
        // 1-6: jump to an area (with the lens open, the keys switch features).
        const std::vector<Area>& areas = Areas();
        for (std::size_t i = 0; !m_lens && i < areas.size(); ++i)
        {
            if (input.WasKeyPressed(static_cast<SDL_Scancode>(SDL_SCANCODE_1 + i)))
            {
                const Level* level = m_levels->GetLevel();
                const auto found = level->GetData().spawns.find(areas[i].spawn);
                if (found != level->GetData().spawns.end())
                {
                    Teleport(found->second.position, found->second.yawDegrees);
                }
            }
        }
        // P: skip ahead to the next time of day; T: stop or restart the clock.
        if (input.WasKeyPressed(SDL_SCANCODE_P))
        {
            AdvanceClock(2.0f);
        }
        if (input.WasKeyPressed(SDL_SCANCODE_T))
        {
            m_clockOn = !m_clockOn;
        }
    }

    const std::vector<std::string>& ShowcaseApp::Day()
    {
        // The village's day: each time holds, then blends into the next.
        static const std::vector<std::string> day = { "clear_day", "sunset", "night", "rain", "overcast" };
        return day;
    }

    void ShowcaseApp::UpdateClock(float deltaSeconds)
    {
        if (!m_clockOn || m_environment.IsTransitioning())
        {
            return;
        }
        m_clockTime += deltaSeconds;
        if (m_clockTime >= m_holdSeconds)
        {
            AdvanceClock(BlendSeconds);
        }
    }

    void ShowcaseApp::AdvanceClock(float blendSeconds)
    {
        const std::vector<std::string>& day = Day();
        const auto current = std::find(day.begin(), day.end(), m_environmentName);
        const std::size_t next = current == day.end() ? 0 : (current - day.begin() + 1) % day.size();
        m_clockTime = 0.0f;
        SetEnvironment(day[next], blendSeconds);
    }

    void ShowcaseApp::UpdatePavilion()
    {
        // The character runs the lab's demonstration loop by itself: idle,
        // walk, run, a jump (LabViewer::Demo, M38) - its state machine and
        // blends decide the pose.
        if (Entity* rudy = FindEntity("rudy"); rudy && rudy->animator)
        {
            const LabViewer::DemoParams demo = LabViewer::Demo(std::fmod(m_time, LabViewer::DemoSeconds));
            rudy->animator->SetParam("speed", demo.speed);
            rudy->animator->SetParam("grounded", demo.grounded ? 1.0f : 0.0f);
        }
    }

    void ShowcaseApp::UpdateBooth()
    {
        // The keys play only at the booth.
        const Area* area = AreaAt(m_player.GetFeetPosition());
        if (!area || std::string_view(area->spawn) != "radio")
        {
            return;
        }
        const Atom::Input& input = GetInput();
        constexpr SDL_Scancode keys[4] = { SDL_SCANCODE_J, SDL_SCANCODE_K, SDL_SCANCODE_L, SDL_SCANCODE_SEMICOLON };
        for (int i = 0; i < 4; ++i)
        {
            if (input.WasKeyPressed(keys[i]))
            {
                SendToBooth(BoothCommand::Note, i, 0.0f);
            }
        }
        if (input.WasKeyPressed(SDL_SCANCODE_U) || input.WasKeyPressed(SDL_SCANCODE_I))
        {
            const float factor = input.WasKeyPressed(SDL_SCANCODE_I) ? 1.5f : 1.0f / 1.5f;
            m_cutoff = std::clamp(m_cutoff * factor, BoothSynth::MinCutoff, BoothSynth::MaxCutoff);
            SendToBooth(BoothCommand::Filter, 0, m_cutoff);
        }
        if (input.WasKeyPressed(SDL_SCANCODE_O))
        {
            m_delay = !m_delay;
            SendToBooth(BoothCommand::Delay, m_delay ? 1 : 0, 0.0f);
        }
    }

    void ShowcaseApp::SendToBooth(BoothCommand command, int argument, float value)
    {
        if (!m_audio)
        {
            m_audio = std::make_unique<Atom::SynthStream>(); // heap: the delay line is large
            if (!m_audio->Start(m_booth))
            {
                std::cout << "Synth booth: no audio device - playing silent\n";
            }
            else
            {
                m_audio->SetGain(m_store.Settings().volume / 0.8f); // the player's volume (M90)
            }
            if (m_audio->IsRunning() && !m_featureOn[FeatureIndex("synth")])
            {
                m_audio->Send({ static_cast<int>(BoothCommand::Mute), 1, 0.0f }); // switched off in the lens
            }
        }
        if (m_audio->IsRunning())
        {
            m_audio->Send({ static_cast<int>(command), argument, value });
        }
    }

    void ShowcaseApp::UpdateMouseCapture()
    {
        Atom::Input& input = GetInput();
        // Escape opens the menu (M90, UpdateScreens), which releases the
        // mouse; a click back in the village recaptures it.
        if (m_screen == Screen::None && !input.IsMouseCaptured() && !GetDevTools().IsVisible()
                 && (SDL_GetMouseState(nullptr, nullptr) & SDL_BUTTON_LMASK))
        {
            input.SetMouseCaptured(GetWindow().GetSDLWindow(), true);
        }
    }

    const ShowcaseApp::Area* ShowcaseApp::AreaAt(const glm::vec3& feet) const
    {
        const Level* level = m_levels->GetLevel();
        const Area* nearest = nullptr;
        float best = 16.0f; // metres: beyond this, between areas
        for (const Area& area : Areas())
        {
            const auto found = level->GetData().spawns.find(area.spawn);
            if (found == level->GetData().spawns.end())
            {
                continue;
            }
            const glm::vec3 offset = found->second.position - feet;
            const float distance = std::sqrt(offset.x * offset.x + offset.z * offset.z);
            if (distance < best)
            {
                best = distance;
                nearest = &area;
            }
        }
        return nearest;
    }

    void ShowcaseApp::DrawCaption()
    {
        // Bottom left: the place, what's here; under it, the keys (the kit's caption).
        const Area* area = m_levels->GetLevel() ? AreaAt(m_player.GetFeetPosition()) : nullptr;
        const std::string title = area ? area->title : "By the lake";
        std::string shows = area ? area->shows : "1-5 go to a place";
        if (area && std::string_view(area->spawn) == "radio")
        {
            char state[64];
            std::snprintf(state, sizeof(state), "   (tone %.0f Hz, echo %s)", m_cutoff, m_delay ? "on" : "off");
            shows += state;
        }
        const std::string keys = "1-5 places    P " + EnvironmentName() + (m_clockOn ? "" : " (paused)")
            + "    T pause time    Tab the lens    F1 numbers    F10 tools";
        UiKit(GetRenderer().GetUI(), *m_font, *m_smallFont).Caption(title, shows, keys);
    }

    const Entity* ShowcaseApp::FindEntity(const std::string& name) const
    {
        return const_cast<ShowcaseApp*>(this)->FindEntity(name);
    }

    Entity* ShowcaseApp::FindEntity(const std::string& name)
    {
        Level* level = m_levels ? m_levels->GetLevel() : nullptr;
        Entity* found = nullptr;
        if (level)
        {
            level->GetWorld().ForEach([&](EntityId, Entity& entity) {
                if (!found && entity.name == name)
                {
                    found = &entity;
                }
            });
        }
        return found;
    }

    const Atom::CollisionWorld* ShowcaseApp::CurrentCollision() const
    {
        const Level* level = m_levels ? m_levels->GetLevel() : nullptr;
        return level ? &level->GetCollision() : nullptr;
    }

    void ShowcaseApp::OnShutdown()
    {
        m_audio.reset(); // the audio thread stops before the booth it plays goes
        GetRenderer().SetParticleAtlas(nullptr, 1); // the atmosphere's atlas goes next
        m_levels.reset(); // the level's GPU resources go before the device
        m_smallFont.reset();
        m_font.reset();
        m_atmosphere.Shutdown();
    }

    // --- The scenario harness --------------------------------------------

    bool ShowcaseApp::TeleportTo(const std::string& name, float distance)
    {
        const Entity* entity = FindEntity(name);
        if (!entity)
        {
            return false;
        }
        const glm::vec3 focus = entity->position + glm::vec3{ 0.0f, 1.2f, 0.0f };
        glm::vec3 away = m_player.GetFeetPosition() - focus;
        away.y = 0.0f;
        const float length = glm::length(away);
        away = length > 0.01f ? away / length : glm::vec3{ 0.0f, 0.0f, 1.0f };
        glm::vec3 feet = focus + away * distance;
        if (const Atom::CollisionWorld* collision = CurrentCollision())
        {
            feet.y = collision->FindFloor({ feet.x, focus.y + 3.0f, feet.z }, 20.0f).value_or(0.0f);
        }
        m_player.Teleport(feet, m_camera);
        return Face(name);
    }

    void ShowcaseApp::Teleport(const glm::vec3& feet, float yawDegrees)
    {
        if (m_mode == Mode::Driving)
        {
            // Drive mode: the character moves, and the camera looks the given
            // way (forward walks along it).
            m_driveBody.Place(feet);
            m_arm.Reset(-yawDegrees, m_arm.GetPitchDegrees());
            return;
        }
        m_player.Teleport(feet, m_camera);
        m_camera.SetRotation(glm::radians(yawDegrees), 0.0f);
    }

    bool ShowcaseApp::Face(const std::string& name)
    {
        const Entity* entity = FindEntity(name);
        if (!entity)
        {
            return false;
        }
        const glm::vec3 eye = m_player.GetFeetPosition() + glm::vec3{ 0.0f, m_player.eyeHeight, 0.0f };
        const glm::vec3 offset = entity->position + glm::vec3{ 0.0f, 1.2f, 0.0f } - eye;
        m_camera.SetRotation(std::atan2(offset.x, -offset.z),
                             std::atan2(offset.y, std::sqrt(offset.x * offset.x + offset.z * offset.z)));
        return true;
    }

    std::string ShowcaseApp::LevelName() const
    {
        const Level* level = m_levels ? m_levels->GetLevel() : nullptr;
        return level ? level->GetName() : std::string{};
    }

    std::string ShowcaseApp::ModeName() const
    {
        if (m_levels && m_levels->IsTransitioning())
        {
            return "transitioning";
        }
        return m_mode == Mode::Viewing ? "viewer" : m_mode == Mode::Driving ? "drive" : "exploring";
    }

    std::size_t ShowcaseApp::VoiceCount() const
    {
        return const_cast<ShowcaseApp*>(this)->GetAudio().GetLoopingVoiceCount();
    }

    ArrivalError ShowcaseApp::Arrival() const
    {
        const float yaw = std::remainder(m_camera.GetYaw() - m_arrivalYaw, glm::two_pi<float>());
        return ArrivalError{ glm::length(m_camera.GetPosition() - m_arrivalEye), std::abs(glm::degrees(yaw)) };
    }

    std::optional<float> ShowcaseApp::AnimationTime(const std::string& name) const
    {
        const Entity* entity = FindEntity(name);
        return entity && entity->animated ? std::optional<float>(entity->animated->time) : std::nullopt;
    }

    bool ShowcaseApp::AnimationPlaying(const std::string& name) const
    {
        const Entity* entity = FindEntity(name);
        return entity && entity->animated && entity->animated->playing;
    }

    std::string ShowcaseApp::AnimatorState(const std::string& name) const
    {
        const Entity* entity = FindEntity(name);
        return entity && entity->animator ? entity->animator->GetStateName() : std::string{};
    }

    bool ShowcaseApp::SetEnvironment(const std::string& name, float seconds)
    {
        const std::string preset = name == "level" ? std::string() : name;
        if (!preset.empty() && !m_presets.Has(preset))
        {
            return false;
        }
        m_environmentName = preset;
        m_environment.SwitchTo(m_presets.Resolve(m_levels->GetLevel(), preset), seconds);
        ApplyLighting();
        return true;
    }

    std::string ShowcaseApp::EnvironmentName() const
    {
        if (m_environment.IsTransitioning())
        {
            return "(blending)";
        }
        return m_environmentName.empty() ? "level" : m_environmentName;
    }

    std::uint32_t ShowcaseApp::ParticleCount() const
    {
        return const_cast<ShowcaseApp*>(this)->GetRenderer().GetLastFrameStats().particles;
    }

    std::uint32_t ShowcaseApp::ReflectionDraws() const
    {
        return const_cast<ShowcaseApp*>(this)->GetRenderer().GetLastFrameStats().reflectionDrawn;
    }

    std::uint32_t ShowcaseApp::WaterDraws() const
    {
        return const_cast<ShowcaseApp*>(this)->GetRenderer().GetLastFrameStats().waterDraws;
    }

    std::string ShowcaseApp::ReloadLevel()
    {
        return m_levels->Reload();
    }

    void ShowcaseApp::RequestLevel(const std::string& level, const std::string& spawn)
    {
        m_levels->RequestChange(level, spawn);
    }

    void ShowcaseApp::Log(const std::string& text)
    {
        std::cout << "[test] " << text << '\n';
    }

    std::string ShowcaseApp::Capture(const std::string& stem, bool includeUi)
    {
        const std::string path = m_outputRoot + "out/img/" + stem + ".png";
        GetRenderer().RequestCapture(path, includeUi);
        return path;
    }

    bool ShowcaseApp::CapturePending() const
    {
        return const_cast<ShowcaseApp*>(this)->GetRenderer().IsCapturePending();
    }

    bool ShowcaseApp::Set(const std::string& what, const std::string& value)
    {
        // The switches scenarios, benchmarks and documentation captures
        // flip - the demo's names and meanings, for what the Showcase has.
        const bool on = value == "on";
        const bool onOff = on || value == "off";
        char* end = nullptr;
        const float number = std::strtof(value.c_str(), &end);
        const bool isNumber = end && *end == '\0' && !value.empty();
        // The view switches every app shares (the framework's, M89).
        if (ApplyViewSwitch(GetRenderer(), m_view, &m_camera, &m_atmosphere, what, value))
        {
            ApplyLighting();
            return true;
        }
        bool handled = true;
        if (what == "overlay" && onOff) GetDevTools().SetOverlayVisible(on);
        else if (what == "devtools" && onOff) GetDevTools().SetVisible(on);
        else if (what == "devtools_collapsed" && onOff) m_devPanels.CollapseNext(on);
        else if (what == "hud" && onOff) m_showHud = on;
        else if (what == "world" && onOff) m_drawWorld = on;
        else if (what == "unease" && value == "off") {} // the demo's moments; the Showcase has none
        else if (what == "lens" && onOff) m_lens = on;
        else if (what == "clock" && onOff) m_clockOn = on;
        else if (what == "menu" && onOff) { if (on) OpenMenu(); else m_screen = Screen::None; }
        else if (what == "menu" && value == "settings") { m_settingsScreen = SettingsScreen(); m_screen = Screen::Settings; }
        else if (what == "benchmark" && value == "start") StartBenchmark();
        else if (what == "benchmark_seconds" && isNumber && number >= 2.0f) m_benchmarkSeconds = number;
        else if (what == "clock" && value == "next") AdvanceClock(0.0f);
        else if (what == "clock_hold" && isNumber && number >= 0.5f) m_holdSeconds = number;
        else if (what.rfind("feature_", 0) == 0 && onOff && FeatureIndex(std::string_view(what).substr(8)) >= 0)
        {
            const int index = FeatureIndex(std::string_view(what).substr(8));
            SetFeature(index, on);
            m_lensFocus = index;
        }
        else if (what == "fixed_dt" && isNumber && number >= 0.0f && number <= 0.25f)
        {
            m_diagnostics.SetFixedStep(number);
        }
        else
        {
            handled = false;
        }
        if (handled)
        {
            ApplyLighting();
            return true;
        }
        // The character lab's switches, while it is open.
        if (m_mode != Mode::Walking)
        {
            if (what == "mode" && (value == "clips" || value == "blend" || value == "machine"))
            {
                m_viewer.SelectMode(value == "clips" ? ViewerMode::Clips
                                    : value == "blend" ? ViewerMode::Blend : ViewerMode::StateMachine);
                ApplyLabPose();
                return true;
            }
            if (what == "blend" && isNumber && number >= 0.0f && number <= 1.0f)
            {
                m_viewer.SetBlendWeight(number);
                ApplyLabPose();
                return true;
            }
            if ((what == "skeleton" || what == "weights" || what == "bind" || what == "pause") && onOff)
            {
                if (what == "skeleton") m_viewer.SetSkeleton(on);
                else if (what == "weights") m_viewer.SetWeights(on);
                else if (what == "bind") m_viewer.SetBindPose(on);
                else m_viewer.SetPaused(on);
                ApplyLabPose();
                return true;
            }
        }
        return false;
    }
}
