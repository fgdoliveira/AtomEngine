#include "ArenaApp.h"

#include "Assets/Model.h"
#include "Core/Profile.h"
#include "Platform/Input.h"
#include "Platform/Window.h"
#include "Renderer/Renderer.h"
#include "UI/UIRenderer.h"
#include "UI/UiKit.h"

#include <SDL3/SDL.h>
#include <imgui.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/trigonometric.hpp>

#include <cstdio>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace Arena
{
    namespace
    {
        constexpr int EnemyPresets[] = { 50, 100, 250, 500, 1000 };
        constexpr int ProjectilePresets[] = { 0, 100, 500, 1000, 2000 };

        double NowMs()
        {
            return static_cast<double>(SDL_GetPerformanceCounter()) * 1000.0
                / static_cast<double>(SDL_GetPerformanceFrequency());
        }
    }

    bool ArenaApp::OnInitialize()
    {
        SDL_SetWindowTitle(GetWindow().GetSDLWindow(), "ATOM ARENA");
        const char* base = SDL_GetBasePath();
        m_root = base ? base : "";
        m_assets = AssetRoots({ m_root + "Assets/AtomArena/", m_root + "Assets/" });

        Atom::Renderer& renderer = GetRenderer();
        m_levels = std::make_unique<LevelManager>(
            Level::Services{ renderer, GetAudio(), m_sounds, m_assets, m_models, ScreenFactory{} });
        m_levels->onLoaded = [this](Level&, const SpawnPoint& spawn) {
            m_player.Teleport(spawn.position, m_camera);
            m_camera.SetRotation(glm::radians(spawn.yawDegrees), 0.0f);
            ApplyLighting();
        };
        if (!m_levels->Load("arena", ""))
        {
            return false;
        }
        // The enemies' model, shared by all of them; projectiles are white
        // billboards tinted orange (a 1×1 atlas, as DRIFT's).
        m_enemyModel = m_models.Get(renderer, m_assets.Resolve("Kit/keeper.glb"));
        const std::uint8_t white[4]{ 255, 255, 255, 255 };
        m_white = renderer.CreateTexture(1, 1, white, false);
        renderer.SetParticleAtlas(m_white.get(), 1);
        m_fonts = std::make_unique<UiFonts>(renderer, m_assets.Resolve("Fonts/ShipporiMincho-Medium-Latin.ttf"));
        if (!m_enemyModel)
        {
            return false;
        }

        m_config.enemies = 250;
        m_config.projectiles = 500;
        Restart();
        m_panels.Add("Arena", 1, [this] { DrawArenaPanel(); });

        m_hooks = std::make_unique<Hooks>(BasicTestHooks::View{ &renderer, &GetAudio(), m_levels.get(), &m_player,
                                                                &m_camera, &m_view, m_root }, *this);
        if (const std::optional<int> exitCode = m_diagnostics.InitializeFromEnvironment())
        {
            RequestQuit(*exitCode);
        }
        std::cout << "ATOM ARENA: 1-5 enemies, P projectiles, C collision search, B spawn batch, R restart,"
                     " V view, F1 numbers, F10 tools (Arena panel), Esc quit\n";
        m_options = ParseBenchmarkArguments(m_arguments);
        for (const std::string& error : m_options.errors)
        {
            std::cerr << "ATOM ARENA: " << error << '\n';
        }
        if (!m_options.errors.empty())
        {
            return false;
        }
        if (m_options.benchmark)
        {
            StartBenchmark();
            return true; // no mouse needed: nobody plays
        }
        const bool captured = GetInput().SetMouseCaptured(GetWindow().GetSDLWindow(), true);
        return captured || m_diagnostics.HasTestScript();
    }

    void ArenaApp::Restart()
    {
        m_sim.Reset(m_config);
        m_step.accumulator = 0.0f;
        m_simMs.Clear();
    }

    void ArenaApp::OnUpdate(float deltaSeconds)
    {
        const double updateStart = NowMs();
        const float frameMs = deltaSeconds * 1000.0f; // the whole last frame, real
        m_diagnostics.RecordFrameTime(deltaSeconds, "arena");
        deltaSeconds = m_diagnostics.Step(deltaSeconds);
        if (const std::optional<int> exitCode = m_diagnostics.UpdateTestScript(deltaSeconds, *m_hooks))
        {
            RequestQuit(*exitCode);
        }
        if (GetInput().WasKeyPressed(SDL_SCANCODE_ESCAPE))
        {
            RequestQuit();
        }
        if (HandleViewKeys(GetInput(), GetRenderer(), m_view, nullptr))
        {
            ApplyLighting();
        }
        UpdateKeys();

        m_levels->Update(deltaSeconds);
        Simulate(deltaSeconds);
        UpdateCamera(deltaSeconds);
        const double submitStart = NowMs();
        SubmitArena();
        m_lastSubmitMs = NowMs() - submitStart;
        if (!m_scenario)
        {
            DrawHud();
        }

        DevContext tools;
        tools.renderer = &GetRenderer();
        tools.tools = &GetDevTools();
        tools.timing = &GetAverageFrameTiming(); // M99
        tools.camera = &m_camera;
        tools.view = &m_view;
        tools.levels = m_levels.get();
        tools.set = [this](const std::string& what, const std::string& value) { return m_hooks->Set(what, value); };
        tools.applyLighting = [this] { ApplyLighting(); };
        m_panels.Draw(tools, deltaSeconds);
        if (m_scenario && frameMs > 0.0f) // M98: not the frame drawn during loading
        {
            UpdateBenchmark(NowMs() - updateStart, frameMs);
        }
    }

    void ArenaApp::Simulate(float deltaSeconds)
    {
        // Fixed 60 Hz steps (the same simulation at any frame rate); the
        // time they take is real time, measured around them. A benchmark
        // runs exactly one step per frame: the same work every frame.
        m_spawnedThisFrame = false;
        if (m_scenario && m_scenario->spikeBatch > 0 && m_benchmarkFrame > 0)
        {
            // D: the batch arrives every period, and leaves `length` later.
            if (m_benchmarkFrame % m_scenario->spikePeriod == 0)
            {
                const double spawnStart = NowMs();
                m_sim.SpawnBatch(m_scenario->spikeBatch);
                m_lastSpawnMs = NowMs() - spawnStart;
                m_spawnedThisFrame = true;
            }
            else if (m_benchmarkFrame % m_scenario->spikePeriod == m_scenario->spikeLength)
            {
                m_sim.Truncate(m_scenario->config.enemies);
            }
        }
        const int steps = m_scenario ? 1 : m_step.Advance(deltaSeconds);
        const double start = NowMs();
        for (int i = 0; i < steps; ++i)
        {
            m_sim.Step(m_step.step);
        }
        if (steps > 0)
        {
            m_lastSimMs = static_cast<float>(NowMs() - start);
            m_simMs.AddSample(m_lastSimMs);
            if (m_simMs.Count() > 120)
            {
                m_simMs.Clear(); // a rolling window, about two seconds
            }
        }
        if (GetDevTools().IsOverlayVisible())
        {
            char line[160];
            std::snprintf(line, sizeof(line), "Arena: %zu enemies, %zu projectiles, %zu contacts, sim %.3f ms (%s)",
                          m_sim.Enemies().size(), m_sim.Projectiles().size(), m_sim.EnemyContacts().size(),
                          m_lastSimMs, m_sim.Config().collision == CollisionMode::Grid ? "grid" : "naive");
            GetDevTools().AddOverlayLine(line);
        }
    }

    void ArenaApp::UpdateKeys()
    {
        const Atom::Input& input = GetInput();
        for (int i = 0; i < 5; ++i)
        {
            if (input.WasKeyPressed(static_cast<SDL_Scancode>(SDL_SCANCODE_1 + i)))
            {
                m_hooks->Set("arena_enemies", std::to_string(EnemyPresets[i]));
            }
        }
        if (input.WasKeyPressed(SDL_SCANCODE_P))
        {
            int next = 0;
            for (int i = 0; i < 5; ++i)
            {
                if (ProjectilePresets[i] == m_config.projectiles) next = (i + 1) % 5;
            }
            m_hooks->Set("arena_projectiles", std::to_string(ProjectilePresets[next]));
        }
        if (input.WasKeyPressed(SDL_SCANCODE_C))
        {
            m_hooks->Set("arena_collision", m_config.collision == CollisionMode::Grid ? "naive" : "grid");
        }
        if (input.WasKeyPressed(SDL_SCANCODE_B))
        {
            m_hooks->Set("arena_spawn", std::to_string(m_spawnBatch));
        }
        if (input.WasKeyPressed(SDL_SCANCODE_R))
        {
            Restart();
        }
        if (input.WasKeyPressed(SDL_SCANCODE_V))
        {
            m_overview = !m_overview;
        }
    }

    void ArenaApp::UpdateCamera(float deltaSeconds)
    {
        if (m_overview)
        {
            // A fixed view from above one side: the whole floor in frame.
            m_camera.SetPosition({ 0.0f, 13.0f, 21.0f });
            m_camera.SetRotation(0.0f, glm::radians(-32.0f));
            return;
        }
        Level* level = m_levels->GetLevel();
        const Atom::Input& input = GetInput();
        const auto held = [&](SDL_Scancode key) { return input.IsKeyDown(key) ? 1.0f : 0.0f; };
        PlayerController::MoveIntent intent;
        intent.move = { held(SDL_SCANCODE_D) - held(SDL_SCANCODE_A), held(SDL_SCANCODE_W) - held(SDL_SCANCODE_S) };
        intent.jog = input.IsKeyDown(SDL_SCANCODE_LSHIFT);
        m_player.Update(input, intent, m_camera, level ? &level->GetCollision() : nullptr, deltaSeconds);
    }

    void ArenaApp::SubmitArena()
    {
        ATOM_PROFILE_SCOPE("ArenaSubmit");
        Atom::Renderer& renderer = GetRenderer();
        renderer.SetCamera(m_camera.GetViewMatrix(), m_camera.verticalFov, m_camera.nearPlane, 120.0f);
        if (Level* level = m_levels->GetLevel())
        {
            level->Update(0.0f, m_player.GetFeetPosition());
            level->Submit(renderer, m_player.GetFeetPosition());
        }
        // E: a draw per enemy, facing where it walks.
        if (m_drawEnemies)
        {
            ATOM_PROFILE_SCOPE("EnemyDraws"); // E: a Model::Submit per enemy
            for (const Enemy& enemy : m_sim.Enemies())
            {
                const float yaw = glm::dot(enemy.velocity, enemy.velocity) > 1e-6f
                    ? std::atan2(enemy.velocity.x, enemy.velocity.y) : 0.0f;
                glm::mat4 transform = glm::translate(glm::mat4{ 1.0f }, { enemy.position.x, 0.0f, enemy.position.y });
                transform = glm::rotate(transform, yaw, { 0.0f, 1.0f, 0.0f });
                m_enemyModel->Submit(renderer, transform);
            }
        }
        // C: projectiles as glowing billboards at chest height.
        m_projectileParticles.clear();
        for (const Projectile& projectile : m_sim.Projectiles())
        {
            Atom::Particle p{};
            p.position = { projectile.position.x, 1.1f, projectile.position.y };
            p.size = 0.22f;
            p.color = { 2.2f, 0.8f, 0.2f, 1.0f }; // a little over 1: they glow
            m_projectileParticles.push_back(p);
        }
        if (!m_projectileParticles.empty())
        {
            renderer.SubmitParticles(m_projectileParticles);
        }
    }

    void ArenaApp::DrawHud()
    {
        UiKit kit(GetRenderer().GetUI(), *m_fonts);
        if (!kit.IsReady())
        {
            return;
        }
        char line[200];
        std::snprintf(line, sizeof(line), "%zu enemies   %zu projectiles   %zu contacts   %llu hits   sim %.2f ms (%s)",
                      m_sim.Enemies().size(), m_sim.Projectiles().size(), m_sim.EnemyContacts().size(),
                      static_cast<unsigned long long>(m_sim.TotalHits()), m_simMs.Median(),
                      m_sim.Config().collision == CollisionMode::Grid ? "grid" : "naive");
        kit.Caption("ATOM ARENA", line,
                    "1-5 enemies   P projectiles   C collision   B spawn 500   R restart   V view   F1   F10");
    }

    void ArenaApp::DrawArenaPanel()
    {
        // F10 > Arena: every workload, sized and switched; applied through
        // the same Set() the scenarios use.
        int enemies = m_config.enemies;
        if (ImGui::SliderInt("Enemies", &enemies, 0, 2000))
        {
            m_hooks->Set("arena_enemies", std::to_string(enemies));
        }
        int projectiles = m_config.projectiles;
        if (ImGui::SliderInt("Projectiles", &projectiles, 0, 4000))
        {
            m_hooks->Set("arena_projectiles", std::to_string(projectiles));
        }
        int mode = m_config.collision == CollisionMode::Grid ? 1 : 0;
        if (ImGui::Combo("Collision", &mode, "naive (all pairs)\0grid\0"))
        {
            m_hooks->Set("arena_collision", mode == 1 ? "grid" : "naive");
        }
        const auto toggle = [&](const char* label, const char* what, bool value) {
            if (ImGui::Checkbox(label, &value))
            {
                m_hooks->Set(what, value ? "on" : "off");
            }
        };
        toggle("A  enemy update", "arena_update", m_config.updateEnemies);
        toggle("B  collision", "arena_collide", m_config.collide);
        toggle("C  projectiles", "arena_projectiles_move", m_config.updateProjectiles);
        toggle("E  draw enemies", "arena_draw", m_drawEnemies);
        ImGui::SliderInt("Batch", &m_spawnBatch, 10, 2000);
        ImGui::SameLine();
        if (ImGui::Button("D  spawn"))
        {
            m_hooks->Set("arena_spawn", std::to_string(m_spawnBatch));
        }
        if (ImGui::Button("Restart (seed)"))
        {
            Restart();
        }
        ImGui::Text("sim median %.3f ms, p95 %.3f ms, last %.3f ms", m_simMs.Median(), m_simMs.Percentile(0.95),
                    m_lastSimMs);
        ImGui::Text("%zu contacts, %llu hits, step %llu", m_sim.EnemyContacts().size(),
                    static_cast<unsigned long long>(m_sim.TotalHits()), static_cast<unsigned long long>(m_sim.Steps()));
    }

    void ArenaApp::ApplyLighting()
    {
        const Level* level = m_levels->GetLevel();
        GetRenderer().SetLighting(SceneLightingFor(level, m_presets.Resolve(level, ""), m_view));
    }

    void ArenaApp::OnShutdown()
    {
        GetRenderer().SetParticleAtlas(nullptr, 1);
        m_white.reset();
        m_enemyModel.reset();
        m_fonts.reset();
        m_levels.reset(); // the level's GPU resources go before the device
    }

    // --- The benchmark (M94) ----------------------------------------------

    void ArenaApp::StartBenchmark()
    {
        m_scenario = FindScenario(m_options.scenario);
        m_config = m_scenario->config;
        m_config.seed = m_options.seed;
        m_drawEnemies = m_scenario->drawEnemies;
        m_overview = true;
        Restart();
        std::cout << "Benchmark: " << m_scenario->name << ", seed " << m_options.seed << ", " << m_options.warmup
                  << " warm-up + " << m_options.frames << " frames\n";
    }

    void ArenaApp::UpdateBenchmark(double updateMs, float frameMs)
    {
        ++m_benchmarkFrame;
        if (m_benchmarkFrame > m_options.warmup + 1) // the first measured frame's period follows a warm-up frame
        {
            m_frameMs.AddSample(frameMs);
            m_updateMs.AddSample(updateMs);
            m_simWindow.AddSample(m_lastSimMs);
            m_cpuMs.AddSample(GetFrameTiming().CpuMs()); // M99: the last completed frame's
            m_gpuWaitMs.AddSample(GetFrameTiming().waitMs);
            m_submitMs.AddSample(m_lastSubmitMs);
            m_contactSum += static_cast<double>(m_sim.EnemyContacts().size());
            if (ATOM_PROFILE_CONNECTED())
            {
                ++m_connectedFrames;
            }
            if (m_spawnedThisFrame)
            {
                m_spikeUpdateMs.AddSample(updateMs);
                m_spawnMs.AddSample(m_lastSpawnMs);
            }
        }
        if (m_benchmarkFrame >= m_options.warmup + 1 + m_options.frames)
        {
            FinishBenchmark();
        }
    }

    void ArenaApp::FinishBenchmark()
    {
        // One `key: value` per line, so runs compare with a diff and
        // Tools/Perf/arena.ps1 reads them back.
        Atom::Renderer& renderer = GetRenderer();
        const Atom::Renderer::DeviceReport device = renderer.GetDeviceReport();
        const Atom::FrameStats& stats = renderer.GetLastFrameStats();
        const glm::vec2 window = renderer.GetUI().GetScreenSize();
        std::string report;
        const auto line = [&](const std::string& key, const std::string& value) { report += key + ": " + value + "\n"; };
        const auto ms = [](double value) {
            char text[32];
            std::snprintf(text, sizeof(text), "%.4f", value);
            return std::string(text);
        };
        const auto distribution = [&](const std::string& key, const Atom::FrameStatsWindow& w) {
            line(key + "_median_ms", ms(w.Median()));
            line(key + "_p95_ms", ms(w.Percentile(0.95)));
            line(key + "_p99_ms", ms(w.Percentile(0.99)));
            line(key + "_worst_ms", ms(w.Percentile(1.0)));
            line(key + "_mean_ms", ms(w.Mean()));
        };
        line("benchmark", "ATOM ARENA");
        line("scenario", m_scenario->name);
        line("version", ATOM_VERSION);
        line("commit", ATOM_GIT_COMMIT); // at configure time
        line("config", ATOM_BUILD_CONFIG);
        // M95: the build's profiler, and how many measured frames it was
        // recording for.
        line("profiling", ATOM_PROFILE_NAME);
        line("profiling_connected_frames", std::to_string(m_connectedFrames));
        line("adapter", device.adapter);
        line("backend", device.backend);
        line("present_mode", device.presentMode);
        line("frames_in_flight", std::to_string(device.framesInFlight));
        line("window", std::to_string(static_cast<int>(window.x)) + "x" + std::to_string(static_cast<int>(window.y)));
        line("scene", std::to_string(stats.sceneWidth) + "x" + std::to_string(stats.sceneHeight));
        line("msaa", std::to_string(stats.msaaSamples));
        line("seed", std::to_string(m_options.seed));
        line("warmup_frames", std::to_string(m_options.warmup));
        line("frames", std::to_string(m_frameMs.Count()));
        line("enemies", std::to_string(m_scenario->config.enemies));
        line("projectiles", std::to_string(m_scenario->config.projectiles));
        line("collision", !m_scenario->config.collide ? "off"
                          : m_scenario->config.collision == CollisionMode::Grid ? "grid" : "naive");
        line("draw_enemies", m_scenario->drawEnemies ? "on" : "off");
        line("spike_batch", std::to_string(m_scenario->spikeBatch));
        line("contacts_mean", ms(m_frameMs.Count() ? m_contactSum / static_cast<double>(m_frameMs.Count()) : 0.0));
        line("draws_last_frame", std::to_string(stats.drawn));
        line("state_hash", std::to_string(m_sim.StateHash()));
        distribution("frame", m_frameMs);   // the whole frame, start to start
        distribution("update", m_updateMs); // the app's CPU work in OnUpdate
        distribution("sim", m_simWindow);   // the simulation step
        distribution("submit", m_submitMs); // the app's draw submission
        distribution("cpu", m_cpuMs);       // M99: the CPU's whole work (update + render)
        distribution("gpu_wait", m_gpuWaitMs); // M99: the CPU waiting for the GPU
        if (m_scenario->spikeBatch > 0)
        {
            line("spikes", std::to_string(m_spikeUpdateMs.Count()));
            distribution("spike_update", m_spikeUpdateMs);
            distribution("spike_spawn", m_spawnMs);
        }

        std::filesystem::path file = m_options.out;
        if (file.empty())
        {
            char stamp[32];
            const std::time_t now = std::time(nullptr);
            std::tm local{};
            localtime_s(&local, &now);
            std::strftime(stamp, sizeof(stamp), "%Y%m%d-%H%M%S", &local);
            file = std::filesystem::path(m_root) / "out" / "arena" / (m_scenario->name + "-" + stamp + ".txt");
        }
        std::error_code error;
        std::filesystem::create_directories(file.parent_path(), error);
        std::ofstream(file, std::ios::binary) << report;
        std::cout << report << "Benchmark written to " << file.string() << '\n';
        RequestQuit(0);
        m_scenario.reset();
    }

    // --- The harness ------------------------------------------------------

    bool ArenaApp::Hooks::Set(const std::string& what, const std::string& value)
    {
        // The arena's switches: a size change restarts from the seed, so a
        // scenario's numbers don't depend on what ran before.
        const bool on = value == "on";
        const bool onOff = on || value == "off";
        char* end = nullptr;
        const long number = std::strtol(value.c_str(), &end, 10);
        const bool isNumber = end && *end == '\0' && !value.empty() && number >= 0;
        ArenaApp& app = m_app;
        if (what == "arena_enemies" && isNumber) { app.m_config.enemies = static_cast<int>(number); app.Restart(); }
        else if (what == "arena_projectiles" && isNumber) { app.m_config.projectiles = static_cast<int>(number); app.Restart(); }
        else if (what == "arena_seed" && isNumber) { app.m_config.seed = static_cast<std::uint32_t>(number); app.Restart(); }
        else if (what == "arena_collision" && (value == "naive" || value == "grid"))
        {
            app.m_config.collision = value == "grid" ? CollisionMode::Grid : CollisionMode::Naive;
            app.m_sim.EditConfig().collision = app.m_config.collision; // live: the same simulation either way
        }
        else if (what == "arena_update" && onOff) app.m_sim.EditConfig().updateEnemies = app.m_config.updateEnemies = on;
        else if (what == "arena_collide" && onOff) app.m_sim.EditConfig().collide = app.m_config.collide = on;
        else if (what == "arena_projectiles_move" && onOff)
        {
            app.m_sim.EditConfig().updateProjectiles = app.m_config.updateProjectiles = on;
        }
        else if (what == "arena_draw" && onOff) app.m_drawEnemies = on;
        else if (what == "arena_view" && (value == "overview" || value == "walk")) app.m_overview = value == "overview";
        else if (what == "arena_spawn" && isNumber) app.m_sim.SpawnBatch(static_cast<int>(number));
        else return BasicTestHooks::Set(what, value);
        return true;
    }

    std::optional<float> ArenaApp::Hooks::Stat(const std::string& name) const
    {
        const ArenaSim& sim = m_app.m_sim;
        if (name == "arena_enemies") return static_cast<float>(sim.Enemies().size());
        if (name == "arena_projectiles") return static_cast<float>(sim.Projectiles().size());
        if (name == "arena_contacts") return static_cast<float>(sim.EnemyContacts().size());
        if (name == "arena_hits") return static_cast<float>(sim.TotalHits());
        if (name == "arena_steps") return static_cast<float>(sim.Steps());
        if (name == "arena_sim_ms") return m_app.m_lastSimMs;
        if (name == "particles") return static_cast<float>(m_app.GetRenderer().GetLastFrameStats().particles);
        return BasicTestHooks::Stat(name);
    }
}
