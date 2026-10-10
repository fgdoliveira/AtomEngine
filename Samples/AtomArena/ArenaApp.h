#pragma once

#include "ArenaBenchmark.h"
#include "ArenaSim.h"

#include "Character/PlayerController.h"
#include "Core/Application.h"
#include "Core/FrameStatsWindow.h"
#include "Debug/DevPanels.h"
#include "Environment/EnvironmentPresets.h"
#include "Level/LevelManager.h"
#include "Level/ModelCache.h"
#include "Level/SoundLibrary.h"
#include "Level/ViewToggles.h"
#include "Platform/AssetRoots.h"
#include "Renderer/Particles.h"
#include "Renderer/Texture.h"
#include "Scene/Camera.h"
#include "Testing/BasicTestHooks.h"
#include "Testing/GameDiagnostics.h"
#include "UI/UiFonts.h"
#include "World/FixedStep.h"

#include <memory>
#include <string>
#include <vector>

namespace Arena
{
    using namespace AtomFramework;

    // ATOM ARENA (v0.0.15, M93): a survival arena at dusk, built to measure.
    // Enemies close in on the centre, projectiles fly out, and each workload
    // - enemy update, collision, projectiles, a spawn spike, drawing - can
    // be switched and sized on its own (F10 > Arena, or the keys), so a
    // profiler can be judged on what it shows about each.
    class ArenaApp : public Atom::Application
    {
    public:
        explicit ArenaApp(std::vector<std::string> arguments) : m_arguments(std::move(arguments)) {}

    protected:
        bool OnInitialize() override;
        void OnUpdate(float deltaSeconds) override;
        void OnShutdown() override;

    public:
        // The harness's hooks, plus the arena's switches and numbers.
        class Hooks : public BasicTestHooks
        {
        public:
            Hooks(View view, ArenaApp& app) : BasicTestHooks(std::move(view)), m_app(app) {}
            bool Set(const std::string& what, const std::string& value) override;
            std::optional<float> Stat(const std::string& name) const override;

        private:
            ArenaApp& m_app;
        };

    private:
        class NoSounds final : public SoundLibrary
        {
        public:
            Atom::SoundHandle GetSound(std::string_view /*name*/) const override { return nullptr; }
        };

        void ApplyLighting();
        void Restart();                  // the simulation again, from its seed
        void UpdateKeys();
        void UpdateCamera(float deltaSeconds);
        void Simulate(float deltaSeconds);
        void SubmitArena();
        void DrawHud();
        void DrawArenaPanel();
        // The benchmark (M94): a named scenario for a fixed number of
        // frames, one simulation step per frame, then a report and quit.
        void StartBenchmark();
        void UpdateBenchmark(double updateMs, float frameMs);
        void FinishBenchmark();
        friend class Hooks;

        std::vector<std::string> m_arguments;
        BenchmarkOptions m_options;
        std::optional<BenchmarkScenario> m_scenario; // set while benchmarking
        int m_benchmarkFrame = 0;                    // counts warm-up frames too
        double m_lastSubmitMs = 0.0;
        double m_lastSpawnMs = 0.0;
        bool m_spawnedThisFrame = false;
        Atom::FrameStatsWindow m_frameMs, m_updateMs, m_simWindow, m_submitMs, m_spikeUpdateMs, m_spawnMs;
        double m_contactSum = 0.0;
        int m_connectedFrames = 0; // M95: measured frames with a profiler capture connected

        std::string m_root;
        AssetRoots m_assets;
        NoSounds m_sounds;
        ModelCache m_models;
        EnvironmentPresets m_presets;
        std::unique_ptr<LevelManager> m_levels;
        PlayerController m_player;
        Atom::Camera m_camera;
        ViewToggles m_view;
        GameDiagnostics m_diagnostics;
        DevPanels m_panels;
        std::unique_ptr<Hooks> m_hooks;
        std::unique_ptr<UiFonts> m_fonts;

        // The simulation, stepped at a fixed 60 Hz; its cost, measured.
        ArenaSim m_sim;
        ArenaConfig m_config;
        FixedStep m_step;
        Atom::FrameStatsWindow m_simMs;   // the last second's simulation times
        float m_lastSimMs = 0.0f;
        int m_spawnBatch = 500;

        // What's drawn: an enemy model per enemy, a billboard per projectile.
        std::shared_ptr<Atom::Model> m_enemyModel;
        std::unique_ptr<Atom::Texture> m_white;
        std::vector<Atom::Particle> m_projectileParticles;
        bool m_drawEnemies = true;        // E: the per-entity draw submission
        bool m_overview = true;           // V: a fixed view over the arena, or walking
    };
}
