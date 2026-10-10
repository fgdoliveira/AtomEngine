// HelloAtom (v0.0.14, M91): the smallest app on AtomEngine and its
// framework - a window, a level from data, walking, the engine's numbers
// (F1), the view keys (F2-F8), the developer tools (F10) and the scenario
// harness. Everything else an app adds goes on top of this.
// docs/Getting-Started.md walks through it.
#include "Character/PlayerController.h"
#include "Core/Application.h"
#include "Debug/DevPanels.h"
#include "Environment/EnvironmentPresets.h"
#include "Level/LevelManager.h"
#include "Level/ModelCache.h"
#include "Level/SoundLibrary.h"
#include "Level/ViewToggles.h"
#include "Platform/AssetRoots.h"
#include "Platform/Input.h"
#include "Platform/Window.h"
#include "Renderer/Renderer.h"
#include "Scene/Camera.h"
#include "Testing/BasicTestHooks.h"
#include "Testing/GameDiagnostics.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h> // the Windows entry point of a windowed program

#include <glm/trigonometric.hpp>

#include <memory>
#include <string>

namespace
{
    using namespace AtomFramework;

    // A level can name sounds; this app has none.
    class NoSounds final : public SoundLibrary
    {
    public:
        Atom::SoundHandle GetSound(std::string_view /*name*/) const override { return nullptr; }
    };

    class HelloApp : public Atom::Application
    {
    protected:
        bool OnInitialize() override
        {
            // 1. Where assets are: this app's own first, then the shared Content/.
            const char* base = SDL_GetBasePath();
            m_root = base ? base : "";
            m_assets = AssetRoots({ m_root + "Assets/HelloAtom/", m_root + "Assets/" });

            // 2. Levels: JSON in Levels/, built into a world of entities.
            m_levels = std::make_unique<LevelManager>(
                Level::Services{ GetRenderer(), GetAudio(), m_sounds, m_assets, m_models, ScreenFactory{} });
            m_levels->onLoaded = [this](Level&, const SpawnPoint& spawn) {
                m_player.Teleport(spawn.position, m_camera);
                m_camera.SetRotation(glm::radians(spawn.yawDegrees), 0.0f);
                ApplyLighting();
            };
            if (!m_levels->Load("hello", ""))
            {
                return false;
            }

            // 3. Scenarios (ATOM_TEST_SCRIPT) and F10: the framework's, given what this app has.
            m_hooks = std::make_unique<BasicTestHooks>(BasicTestHooks::View{
                &GetRenderer(), &GetAudio(), m_levels.get(), &m_player, &m_camera, &m_view, m_root });
            if (const std::optional<int> exitCode = m_diagnostics.InitializeFromEnvironment())
            {
                RequestQuit(*exitCode); // a script that can't run
            }
            const bool captured = GetInput().SetMouseCaptured(GetWindow().GetSDLWindow(), true);
            return captured || m_diagnostics.HasTestScript();
        }

        void OnUpdate(float deltaSeconds) override
        {
            m_diagnostics.RecordFrameTime(deltaSeconds, "hello");
            deltaSeconds = m_diagnostics.Step(deltaSeconds);
            if (const std::optional<int> exitCode = m_diagnostics.UpdateTestScript(deltaSeconds, *m_hooks))
            {
                RequestQuit(*exitCode);
            }

            // Input: Esc quits; F2-F8 switch what's drawn.
            Atom::Input& input = GetInput();
            if (input.WasKeyPressed(SDL_SCANCODE_ESCAPE))
            {
                RequestQuit();
            }
            if (HandleViewKeys(input, GetRenderer(), m_view, nullptr))
            {
                ApplyLighting();
            }

            // Walk: WASD and the mouse, Shift to jog, against the level's collision.
            m_levels->Update(deltaSeconds);
            Level* level = m_levels->GetLevel();
            const auto held = [&](SDL_Scancode key) { return input.IsKeyDown(key) ? 1.0f : 0.0f; };
            PlayerController::MoveIntent intent;
            intent.move = { held(SDL_SCANCODE_D) - held(SDL_SCANCODE_A), held(SDL_SCANCODE_W) - held(SDL_SCANCODE_S) };
            intent.jog = input.IsKeyDown(SDL_SCANCODE_LSHIFT);
            m_player.Update(input, intent, m_camera, level ? &level->GetCollision() : nullptr, deltaSeconds);

            // Draw: the camera, then the level (animations advance in Update).
            Atom::Renderer& renderer = GetRenderer();
            renderer.SetCamera(m_camera.GetViewMatrix(), m_camera.verticalFov, m_camera.nearPlane, m_camera.farPlane);
            if (level)
            {
                level->Update(deltaSeconds, m_player.GetFeetPosition());
                level->Submit(renderer, m_player.GetFeetPosition());
            }

            // F10: the framework's panels, for what this app has.
            DevContext tools;
            tools.renderer = &renderer;
            tools.tools = &GetDevTools();
            tools.camera = &m_camera;
            tools.view = &m_view;
            tools.levels = m_levels.get();
            tools.set = [this](const std::string& what, const std::string& value) { return m_hooks->Set(what, value); };
            tools.applyLighting = [this] { ApplyLighting(); };
            m_panels.Draw(tools, deltaSeconds);
        }

        void OnShutdown() override
        {
            m_levels.reset(); // the level's GPU resources go before the device
        }

    private:
        void ApplyLighting()
        {
            // The level's own light (no environment preset), with the view keys on top.
            const Level* level = m_levels->GetLevel();
            GetRenderer().SetLighting(SceneLightingFor(level, m_presets.Resolve(level, ""), m_view));
        }

        std::string m_root;
        AssetRoots m_assets;
        NoSounds m_sounds;
        ModelCache m_models;
        EnvironmentPresets m_presets; // none loaded: "" resolves to the level's light
        std::unique_ptr<LevelManager> m_levels;
        PlayerController m_player;
        Atom::Camera m_camera;
        ViewToggles m_view;
        GameDiagnostics m_diagnostics;
        DevPanels m_panels;
        std::unique_ptr<BasicTestHooks> m_hooks;
    };
}

int main(int /*argc*/, char** /*argv*/)
{
    HelloApp app;
    return app.Run();
}
