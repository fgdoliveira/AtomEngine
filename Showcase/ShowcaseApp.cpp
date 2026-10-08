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
        // In the order of the number keys.
        static const std::vector<Area> areas = {
            { "plaza", "The plaza", "Meshes, materials, the camera; animated models" },
            { "lakeshore", "The lakeshore", "Water and its reflection, the sky; P switches the weather everywhere" },
            { "lights", "The light corner", "Live lights, glow and halos - best at night (P)" },
            { "pavilion", "The pavilion", "A skinned character: blended clips and a state machine" },
            { "toon_garden", "The toon garden", "Toon shading and outlines beside plain-shaded twins" },
            { "synth_booth", "The synth booth", "The live synth: J K L ; play, U I filter, O delay" },
        };
        return areas;
    }

    bool ShowcaseApp::OnInitialize()
    {
        SDL_SetWindowTitle(GetWindow().GetSDLWindow(), "AtomEngine Showcase");

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

        // ATOM_START_LEVEL=showcase:<spawn> starts at an area (testing).
        std::string startSpawn;
        if (const char* start = Atom::DevSwitch("ATOM_START_LEVEL"))
        {
            const std::string value = start;
            const std::size_t colon = value.find(':');
            startSpawn = colon == std::string::npos ? "" : value.substr(colon + 1);
        }
        if (!m_levels->Load("showcase", startSpawn))
        {
            return false;
        }

        std::cout << "Showcase: 1-6 areas, P weather, WASD/Shift/mouse walk, F1 numbers, F2-F8 view, F10 tools,"
                     " Esc release/quit\n";
        if (const std::optional<int> exitCode = m_diagnostics.InitializeFromEnvironment())
        {
            RequestQuit(*exitCode); // a script that can't run
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
        std::cout << "Entered level '" << data.name << "'\n";
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

        m_levels->Update(deltaSeconds);
        GetRenderer().SetFade(m_levels->GetFade());
        if (m_environment.IsTransitioning())
        {
            m_environment.Update(deltaSeconds);
            ApplyLighting();
        }
        UpdatePavilion();
        if (Level* level = m_levels->GetLevel())
        {
            level->Update(deltaSeconds, m_player.GetFeetPosition());
        }

        if (!m_levels->IsTransitioning())
        {
            UpdateKeys();
            const Atom::Input& input = GetInput();
            const auto held = [&](SDL_Scancode a, SDL_Scancode b) { return input.IsKeyDown(a) || input.IsKeyDown(b); };
            PlayerController::MoveIntent intent;
            intent.move.y = (held(SDL_SCANCODE_W, SDL_SCANCODE_UP) ? 1.0f : 0.0f) - (held(SDL_SCANCODE_S, SDL_SCANCODE_DOWN) ? 1.0f : 0.0f);
            intent.move.x = (held(SDL_SCANCODE_D, SDL_SCANCODE_RIGHT) ? 1.0f : 0.0f) - (held(SDL_SCANCODE_A, SDL_SCANCODE_LEFT) ? 1.0f : 0.0f);
            intent.jog = input.IsKeyDown(SDL_SCANCODE_LSHIFT);
            m_player.Update(input, intent, m_camera, CurrentCollision(), deltaSeconds);
        }

        Atom::Renderer& renderer = GetRenderer();
        renderer.SetCamera(m_camera.GetViewMatrix(), m_camera.verticalFov, m_camera.nearPlane, m_camera.farPlane);
        if (const Level* level = m_levels->GetLevel())
        {
            level->Submit(renderer, m_player.GetFeetPosition());
        }

        // Weather (M50): the environment's wind and, outdoors, its rain.
        const Atom::SceneLighting& lighting = renderer.GetLighting();
        m_atmosphere.SetWind(m_environment.Current().wind);
        m_atmosphere.SetRain(lighting.rain, lighting.skyColor * 0.55f + glm::vec3{ 0.18f });
        m_atmosphere.Update(deltaSeconds, m_player.GetFeetPosition(), lighting.fogColor,
                            lighting.fogDensity > 0.0f ? 1.0f : 0.35f);
        m_atmosphere.Submit(renderer);
        renderer.SetWind(m_atmosphere.GetWind(), m_time);

        DrawCaption();
    }

    void ShowcaseApp::UpdateKeys()
    {
        const Atom::Input& input = GetInput();
        // 1-6: jump to an area.
        const std::vector<Area>& areas = Areas();
        for (std::size_t i = 0; i < areas.size(); ++i)
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
        // P: the next weather, blended over two seconds.
        if (input.WasKeyPressed(SDL_SCANCODE_P))
        {
            const std::vector<std::string> offered = m_presets.Offered(m_levels->GetLevel());
            if (!offered.empty())
            {
                const auto current = std::find(offered.begin(), offered.end(), m_environmentName);
                const std::size_t next = current == offered.end() ? 0 : (current - offered.begin() + 1) % offered.size();
                SetEnvironment(offered[next], 2.0f);
            }
        }
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

    void ShowcaseApp::UpdateMouseCapture()
    {
        Atom::Input& input = GetInput();
        // Escape releases the mouse; again, quits. A click recaptures it.
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
        else if (!input.IsMouseCaptured() && !GetDevTools().IsVisible()
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
        Atom::UIRenderer& ui = GetRenderer().GetUI();
        const glm::vec2 screen = ui.GetScreenSize();
        const float scale = std::clamp(screen.y / 720.0f, 0.75f, 2.0f);
        const float margin = 24.0f * scale;
        const glm::vec4 ink{ 0.94f, 0.93f, 0.88f, 1.0f };
        const glm::vec4 dim{ 0.94f, 0.93f, 0.88f, 0.7f };

        // Bottom left: the area, what it shows; under it, the keys.
        const Area* area = m_levels->GetLevel() ? AreaAt(m_player.GetFeetPosition()) : nullptr;
        const std::string title = area ? area->title : "Between areas";
        const std::string shows = area ? area->shows : "1-6 jump to an area";
        const std::string keys = "1-6 areas    P weather: " + EnvironmentName()
            + "    F1 numbers    F2-F8 view    F10 tools";
        const float smallLine = m_smallFont->GetLineHeight() * scale;
        const float keysY = screen.y - margin - smallLine;
        const float showsY = keysY - smallLine * 1.4f;
        const float titleY = showsY - m_font->GetLineHeight() * scale;
        const glm::vec2 panel{ std::max(ui.MeasureText(*m_smallFont, keys, scale).x,
                                        ui.MeasureText(*m_smallFont, shows, scale).x) + margin,
                               screen.y - titleY + margin * 0.5f };
        ui.DrawRect({ margin * 0.5f, titleY - margin * 0.5f }, { panel.x, panel.y - margin * 0.5f },
                    { 0.04f, 0.04f, 0.05f, 0.55f });
        ui.DrawText(*m_font, title, { margin, titleY }, ink, scale);
        ui.DrawText(*m_smallFont, shows, { margin, showsY }, ink, scale);
        ui.DrawText(*m_smallFont, keys, { margin, keysY }, dim, scale);
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
        return m_levels && m_levels->IsTransitioning() ? "transitioning" : "exploring";
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
        // The switches a scenario may flip: the view toggles, the field of
        // view, rain, the F1 overlay.
        const bool on = value == "on";
        const bool onOff = on || value == "off";
        Atom::Renderer& renderer = GetRenderer();
        Atom::RenderSettings settings = renderer.GetSettings();
        if (what == "shadows" && onOff) { m_view.shadows = on; ApplyLighting(); return true; }
        if (what == "particles" && onOff) { m_atmosphere.SetEnabled(on); return true; }
        if (what == "overlay" && onOff) { GetDevTools().SetOverlayVisible(on); return true; }
        if (what == "msaa" && (value == "1" || value == "2" || value == "4"))
        {
            settings.msaaSamples = static_cast<std::uint32_t>(std::stoi(value));
            renderer.SetSettings(settings);
            return true;
        }
        char* end = nullptr;
        const float number = std::strtof(value.c_str(), &end);
        const bool isNumber = end && *end == '\0' && !value.empty();
        if (what == "fov" && isNumber && number >= 30.0f && number <= 110.0f)
        {
            m_camera.verticalFov = glm::radians(number);
            return true;
        }
        if (what == "rain" && isNumber && number >= 0.0f && number <= 1.0f)
        {
            m_view.rain = number;
            ApplyLighting();
            return true;
        }
        return false;
    }
}
