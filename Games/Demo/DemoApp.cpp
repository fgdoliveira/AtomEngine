#include "DemoApp.h"
#include "Core/DevSwitch.h" // M82: ATOM_* switches, compiled out of packages
#include "Core/AssetLog.h"
#include "Diagnostics/DiagnosticsReport.h"

#include "Pachinko/LevelScreens.h"
#include "Pachinko/PixelDraw.h"

#include "Interaction/ActionExecutor.h"
#include "Interaction/InteractionSystem.h"

#include <SDL3/SDL.h>

#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/mat4x4.hpp>

#include <algorithm>
#include <charconv>
#include <iterator>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>

namespace Demo
{
    Atom::Application::StartupConfig DemoApp::OnConfigure()
    {
        // Before the window and GPU exist (M60): command line > ATOM_*
        // environment > saved settings (M61) > defaults.
        m_commandLine = ParseCommandLine(m_arguments);
        for (const std::string& error : m_commandLine.errors)
        {
            std::cerr << "Command line: " << error << " (ignored)\n";
        }
        EnvironmentOverrides environment;
        if (const char* gpu = Atom::DevSwitch("ATOM_GPU"); gpu && *gpu)
        {
            environment.gpu = ParseGpuPreference(gpu);
            if (!environment.gpu) std::cerr << "ATOM_GPU: unknown value '" << gpu << "' (ignored)\n";
        }
        if (const char* quality = Atom::DevSwitch("ATOM_QUALITY"); quality && *quality)
        {
            environment.quality = ParseQualityMode(quality);
            if (!environment.quality) std::cerr << "ATOM_QUALITY: unknown value '" << quality << "' (ignored)\n";
        }
        // Saved settings (M61) - never for scripted tests, benchmarks or
        // --no-settings: those runs behave the same on every machine.
        LoadSavedSettings();
        const bool useSaved = m_settingsPersist && !m_commandLine.resetSettings;
        m_resolvedSettings = ResolveSettings(m_commandLine, environment, useSaved ? &m_savedSettings : nullptr);
        // Calibration (M64): asked for on the command line or saved as
        // "calibrate next launch" - never in a scripted test run.
        const bool scripted = Atom::DevSwitch("ATOM_TEST_SCRIPT") != nullptr;
        m_calibrateThisRun = !scripted && (m_commandLine.calibrate || (useSaved && m_savedSettings.calibrateNextLaunch));
        if (scripted && m_commandLine.calibrate)
        {
            std::cerr << "--calibrate is ignored in a scripted test run\n";
        }

        StartupConfig config;
        config.gpuPreference = m_resolvedSettings.gpu == GpuPreference::HighPerformance
            ? Atom::GPUPreference::HighPerformance
            : Atom::GPUPreference::LowPower;
        return config;
    }

    void DemoApp::OnRenderFailure(Atom::Renderer::Failure failure)
    {
        // M63: the device failed mid-run. If it was the high-performance
        // adapter, the next launch uses low-power and says why - written
        // before the clean shutdown. No in-process device rebuild.
        const GpuPreference active = GetRenderer().GetActivePreference() == Atom::GPUPreference::HighPerformance
            ? GpuPreference::HighPerformance
            : GpuPreference::LowPower;
        const std::optional<GpuPreference> fallback = FallbackAfterFailure(active);
        if (!fallback)
        {
            return; // already on the known-good adapter: nothing safer to try
        }
        std::cerr << "The high-performance GPU "
                  << (failure == Atom::Renderer::Failure::SwapchainLost ? "lost its swapchain" : "failed")
                  << "; the next launch will use " << ToString(*fallback) << ".\n";
        m_savedSettings.pendingFallback = fallback;
        SaveSettings(); // only when this run uses saved settings (not tests)
    }

    void DemoApp::LoadSavedSettings()
    {
        if (char* pref = SDL_GetPrefPath("AtomEngine", "Demo"))
        {
            m_settingsPath = std::string(pref) + "settings.json";
            SDL_free(pref);
        }
        m_settingsPersist = !m_settingsPath.empty() && !Atom::DevSwitch("ATOM_TEST_SCRIPT") && !m_commandLine.noSettings;
        if (!m_settingsPersist)
        {
            return; // defaults and explicit flags only
        }
        // v0.0.14: the game was AtomGame.exe, its settings (calibration
        // included) under ...\AtomEngine\AtomGame\. Brought over once, when
        // the new folder has none. Remove after a release or two.
        {
            namespace fs = std::filesystem;
            const fs::path current(m_settingsPath);
            const fs::path previous = current.parent_path().parent_path() / "AtomGame" / "settings.json";
            std::error_code error;
            if (!fs::exists(current, error) && fs::exists(previous, error) && fs::copy_file(previous, current, error))
            {
                std::cout << "Settings brought over from " << previous.string() << '\n';
            }
        }
        if (m_commandLine.resetSettings)
        {
            m_savedSettings = GameSettings{};
            SaveSettings();
            std::cout << "Settings reset: " << m_settingsPath << '\n';
            return;
        }
        std::ifstream file(m_settingsPath, std::ios::binary);
        const std::string text{ std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>() };
        SettingsLoad load = ParseSettings(text);
        if (!load.warning.empty())
        {
            std::cerr << m_settingsPath << ": " << load.warning << '\n';
        }
        m_savedSettings = std::move(load.settings);
    }

    std::string DemoApp::PerfContext() const
    {
        Atom::Renderer& renderer = const_cast<DemoApp*>(this)->GetRenderer();
        const Atom::Renderer::DeviceReport report = renderer.GetDeviceReport();
        return "adapter=\"" + report.adapter + "\" present=" + report.presentMode
            + " frames_in_flight=" + std::to_string(report.framesInFlight) + " power=\"" + AtomFramework::PowerStateName()
            + "\" quality=" + std::string(ToString(CurrentQualityTier()));
    }

    bool DemoApp::WriteDiagnostics(const std::string& path) const
    {
        // The shared report (M82, the framework), then the demo's own lines.
        Atom::Renderer& renderer = const_cast<DemoApp*>(this)->GetRenderer();
        SDL_Window* window = const_cast<DemoApp*>(this)->GetWindow().GetSDLWindow();
        const Atom::RenderSettings& s = renderer.GetSettings();

        std::ostringstream out;
        out << AtomFramework::DiagnosticsReport("AtomEngine " ATOM_VERSION " diagnostics", renderer, window,
                                                m_resolvedSettings.gpuReason)
            << "quality.mode: " << ToString(m_resolvedSettings.quality) << "\n"
            << "quality.drawing: " << ToString(CurrentQualityTier()) << "\n"
            << "settings.render_scale: " << s.renderScale << "\n"
            << "settings.msaa: " << s.msaaSamples << "x\n"
            << "settings.shadows: " << (m_view.shadows ? "on" : "off") << "\n"
            << "settings.particles: " << (m_atmosphere.IsEnabled() ? "on" : "off") << "\n"
            << "settings.reflection: " << (renderer.IsReflectionEnabled() ? "allowed" : "off") << "\n"
            << "settings.file: " << (m_settingsPersist ? m_settingsPath : std::string("not used this run")) << "\n";
        return AtomFramework::WriteDiagnosticsFile(path, out.str());
    }

    void DemoApp::SaveSettings()
    {
        if (!m_settingsPersist)
        {
            return;
        }
        std::ofstream file(m_settingsPath, std::ios::binary | std::ios::trunc);
        file << WriteSettings(m_savedSettings);
        if (!file)
        {
            std::cerr << "Could not save settings to " << m_settingsPath << '\n';
        }
    }

    void DemoApp::SetQualityMode(QualityMode mode, bool save)
    {
        m_resolvedSettings.quality = mode;
        ApplyQuality(TierFor(mode, m_savedSettings.calibration, GetRenderer().GetAdapterName()));
        if (save)
        {
            m_savedSettings.quality = mode;
            SaveSettings();
        }
    }

    QualityTier DemoApp::CurrentQualityTier() const
    {
        Atom::Renderer& renderer = const_cast<DemoApp*>(this)->GetRenderer();
        QualityPreset now;
        now.renderScale = renderer.GetSettings().renderScale;
        now.msaaSamples = renderer.GetSettings().msaaSamples;
        now.shadows = m_view.shadows;
        now.particles = m_atmosphere.IsEnabled();
        now.reflection = renderer.IsReflectionEnabled();
        return TierOf(now);
    }

    void DemoApp::ApplyQuality(QualityTier tier)
    {
        // A tier caps features (M60): High allows what levels ask for (the
        // reflection is authored per level), Low turns it off.
        const QualityPreset preset = PresetFor(tier);
        Atom::Renderer& renderer = GetRenderer();
        Atom::RenderSettings settings = renderer.GetSettings();
        settings.renderScale = preset.renderScale;
        settings.msaaSamples = preset.msaaSamples;
        renderer.SetSettings(settings);
        renderer.SetReflectionEnabled(preset.reflection);
        m_view.shadows = preset.shadows;
        m_atmosphere.SetEnabled(preset.particles);
        m_qualityTier = tier;
        ApplyLighting();
    }

    bool DemoApp::OnInitialize()
    {
        const char* basePath = SDL_GetBasePath();
        m_outputRoot = basePath ? basePath : "";
        m_assets = AtomFramework::AssetRoots({ m_outputRoot + "Assets/" });
        // ATOM_ASSET_ROOT=<the repository> reads the source tree instead of
        // the build's copy, and turns on hot reload. v0.0.14: the demo's own
        // assets first, then the shared Content/.
        if (const char* root = Atom::DevSwitch("ATOM_ASSET_ROOT"); root && *root)
        {
            m_outputRoot = root;
            if (m_outputRoot.back() != '/' && m_outputRoot.back() != '\\')
            {
                m_outputRoot += '/';
            }
            m_assets = AtomFramework::AssetRoots({ m_outputRoot + "Games/Demo/Assets/", m_outputRoot + "Content/" });
            m_hotReload = true;
            std::cout << "Hot reload on: assets from " << m_outputRoot << "Games/Demo/Assets and Content\n";
        }

        if (const char* flash = Atom::DevSwitch("ATOM_LATENCY_FLASH"); flash && SDL_strcmp(flash, "1") == 0)
        {
            m_latencyFlash = true;
            std::cout << "Latency flash on: a left click turns that frame black\n";
        }

        m_view.fogPreset = DefaultFogPreset;
        m_audioScape.Initialize(GetAudio());

        if (!m_atmosphere.Initialize(GetRenderer()) || !m_unease.Initialize(GetRenderer()))
        {
            return false;
        }

        // The quality tier (M60), now that the adapter is known.
        ApplyQuality(TierFor(m_resolvedSettings.quality, m_savedSettings.calibration, GetRenderer().GetAdapterName()));
        std::cout << "Quality: " << ToString(m_resolvedSettings.quality) << " -> " << ToString(m_qualityTier)
                  << "; GPU preference: " << ToString(m_resolvedSettings.gpu) << " (" << m_resolvedSettings.gpuReason
                  << "), adapter \"" << GetRenderer().GetAdapterName() << '"' << std::endl; // flushed: a start-up record

        const std::string fontPath = m_assets.Resolve("Fonts/ShipporiMincho-Medium-Latin.ttf");
        m_font = Atom::Font::Load(GetRenderer(), fontPath, 30.0f);
        m_smallFont = Atom::Font::Load(GetRenderer(), fontPath, 19.0f);
        if (!m_font || !m_smallFont)
        {
            return false;
        }

        m_dialogues.LoadDirectory(m_assets.Resolve("Dialogue"));
        LoadFlashlightSettings();
        LoadEnvironmentPresets();

        // Levels get the persistent services they need; the manager tells
        // us when one goes away and when the next one is ready.
        m_levels = std::make_unique<LevelManager>(Level::Services{
            GetRenderer(), GetAudio(), m_audioScape, m_assets, m_modelCache, MakePachinkoScreens() });
        m_levels->onUnloading = [this](Level& outgoing) { OnLevelUnloading(outgoing); };
        m_levels->onLoaded = [this](Level& incoming, const SpawnPoint& spawn) {
            OnLevelLoaded(incoming, spawn);
        };
        m_levels->onReloaded = [this](Level& incoming) { OnLevelReloaded(incoming); };

        // ATOM_START_LEVEL=<name>[:<spawn>] starts somewhere else (testing).
        std::string startLevel = "street";
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

        std::cout
            << "Controls: WASD move, Shift jog, mouse look, E interact, Esc release/quit\n"
            << "  F2 render scale  F3 baked light  F4 MSAA  F5 fog  F6 shadows  F7 post look\n"
            << "  F8 particles  F9 unease events  M mute\n";

        if (const std::optional<int> exitCode = m_diagnostics.InitializeFromEnvironment())
        {
            RequestQuit(*exitCode); // a script that can't run
        }
        // Performance logs say what machine state they measured (M62).
        m_diagnostics.SetPerfContext(PerfContext());

        // M63: say so when the high-performance device couldn't be made or
        // couldn't present to this window - which of the two, and on what.
        if (const auto& fallback = GetRenderer().GetStartupFallback())
        {
            m_resolvedSettings.gpu = GpuPreference::LowPower;
            m_resolvedSettings.gpuReason = "fallback: " + Atom::DescribeFallback(*fallback);
            std::cout << "GPU preference: " << m_resolvedSettings.gpuReason << std::endl;
        }
        if (const char* loss = Atom::DevSwitch("ATOM_SIMULATE_SWAPCHAIN_LOSS"); loss && *loss)
        {
            m_simulateLossAt = static_cast<float>(SDL_atof(loss));
        }
        if (m_calibrateThisRun && !m_commandLine.diagnosticsFile)
        {
            StartCalibration(m_commandLine.calibrate); // --calibrate reports and exits
        }

        // --diagnostics <file> (M62): what this machine gave us, then exit.
        if (m_commandLine.diagnosticsFile)
        {
            const bool written = WriteDiagnostics(*m_commandLine.diagnosticsFile);
            RequestQuit(written ? 0 : 1);
        }

        // A scripted run doesn't need the mouse (and may not have focus).
        const bool captured = GetInput().SetMouseCaptured(GetWindow().GetSDLWindow(), true);
        return captured || m_diagnostics.HasTestScript();
    }

    void DemoApp::OnLevelUnloading(Level& /*outgoing*/)
    {
        // Drop everything that points into the level that's about to die.
        m_target = {};
        m_speaker = {};
        m_sequence.Stop(); // its steps name the old level's entities
        if (m_machine.IsActive())
        {
            m_machine = MachineMode{}; // the machine's screen dies with the level
            m_machineScreen = nullptr;
            m_mode = Mode::Exploring;
        }
        if (m_dialogue.IsActive())
        {
            m_dialogue.Close();
        }
        m_unease.Configure({}, nullptr);
        m_audioScape.SetSurfaceProvider(nullptr);
        m_lab.reset();
        GetRenderer().SetSkinWeightsView(false);
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
        BeginLab(incoming);
        m_mode = RestingMode();
        std::cout << "Entered level '" << data.name << "'\n";
    }

    void DemoApp::OnLevelReloaded(Level& incoming)
    {
        // Same place, same view: only the level's content changed.
        ConfigureForLevel(incoming);
        const LabViewer kept = m_viewer;
        BeginLab(incoming);
        if (m_lab)
        {
            m_viewer = kept; // keep the orbit and the clip across a hot reload
        }
        m_mode = RestingMode();
    }

    void DemoApp::ConfigureForLevel(Level& incoming)
    {
        const LevelData& data = incoming.GetData();
        m_atmosphere.Configure(data.leaves, data.fogBanks, data.dust);
        m_flashlight.EditLight().beam = data.beam; // the air decides if the beam shows
        m_unease.Configure(data.unease, &incoming);
        m_audioScape.SetOutdoor(data.outdoor);
        m_audioScape.SetSurfaceProvider([&incoming](float x, float z) {
            return incoming.GetData().SurfaceAt(x, z);
        });
        ResetEnvironment();
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
        for (const auto& entry : std::filesystem::directory_iterator(m_assets.Resolve("Dialogue"), error))
        {
            if (entry.path().extension() == ".json")
            {
                dialogues.push_back(entry.path().string());
            }
        }
        m_dialogueFiles.Watch(std::move(dialogues));
        m_dataFiles.Watch({ m_assets.Resolve("Data/flashlight.json") });
        std::vector<std::string> presets;
        for (const auto& entry : std::filesystem::directory_iterator(m_assets.Resolve("Environments"), error))
        {
            if (entry.path().extension() == ".json")
            {
                presets.push_back(entry.path().string());
            }
        }
        m_environmentFiles.Watch(std::move(presets));
    }

    void DemoApp::LoadEnvironmentPresets()
    {
        // Every preset in Assets/Environments (M49), checked as it loads: a
        // broken one is reported and left out (the framework's library).
        for (const std::string& problem : m_presets.Load(m_assets.Resolve("Environments")))
        {
            std::cerr << "Environments/" << problem << '\n';
            m_messages.Show(problem);
        }
    }

    EnvironmentState DemoApp::ResolveEnvironment(const std::string& name) const
    {
        // The level's own light, with the preset's values on top.
        return m_presets.Resolve(m_levels ? m_levels->GetLevel() : nullptr, name);
    }

    std::vector<std::string> DemoApp::OfferedPresets() const
    {
        // The level's list; a level without one may try them all.
        return m_presets.Offered(m_levels ? m_levels->GetLevel() : nullptr);
    }

    void DemoApp::ResetEnvironment()
    {
        const Level* level = m_levels ? m_levels->GetLevel() : nullptr;
        m_environmentName.clear();
        if (level && level->GetData().environment)
        {
            const std::string& name = level->GetData().environment->defaultPreset;
            if (m_presets.Has(name))
            {
                m_environmentName = name;
            }
            else
            {
                std::cerr << "Level '" << level->GetName() << "': no environment preset '" << name << "'\n";
            }
        }
        m_environment.Reset(ResolveEnvironment(m_environmentName));
    }

    void DemoApp::RefreshEnvironment()
    {
        m_environment.Reset(ResolveEnvironment(m_environmentName));
    }

    bool DemoApp::SetEnvironment(const std::string& name, float seconds)
    {
        // "level" (or "") is the level's own light.
        const std::string preset = name == "level" ? std::string() : name;
        if (!preset.empty() && !m_presets.Has(preset))
        {
            return false;
        }
        m_environmentName = preset;
        m_environment.SwitchTo(ResolveEnvironment(preset), seconds);
        ApplyLighting();
        return true;
    }

    std::string DemoApp::EnvironmentName() const
    {
        // Mid-blend it isn't any preset yet.
        if (m_environment.IsTransitioning())
        {
            return "(blending)";
        }
        return m_environmentName.empty() ? "level" : m_environmentName;
    }

    void DemoApp::LoadFlashlightSettings()
    {
        // The flashlight's settings (M46); without the file it keeps the
        // values it was built with.
        const std::string path = m_assets.Resolve("Data/flashlight.json");
        Atom::AssetLog::Opened(path);
        std::ifstream file(path, std::ios::binary);
        if (!file)
        {
            return;
        }
        const std::string text{ std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>() };
        if (const std::string error = m_flashlight.LoadSettings(text); !error.empty())
        {
            std::cerr << path << ": " << error << '\n';
            m_messages.Show("flashlight.json: " + error);
        }
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
            m_dialogues.LoadDirectory(m_assets.Resolve("Dialogue"));
            m_messages.Show("Dialogue reloaded");
        }
        if (!m_dataFiles.Poll().empty())
        {
            LoadFlashlightSettings();
            m_messages.Show("Flashlight reloaded");
        }
        if (!m_environmentFiles.Poll().empty())
        {
            LoadEnvironmentPresets();
            RefreshEnvironment();
            ApplyLighting();
            m_messages.Show("Environment reloaded");
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
        // The real frame time, before any fixed step (M57: GameDiagnostics).
        const Level* measured = m_levels ? m_levels->GetLevel() : nullptr;
        m_diagnostics.RecordFrameTime(deltaSeconds, measured ? measured->GetName() : std::string{ "-" });
        deltaSeconds = m_diagnostics.Step(deltaSeconds); // docs: every frame the same step
        UpdateMouseCapture();
        UpdateRenderSettings();
        if (const std::optional<int> exitCode = m_diagnostics.UpdateTestScript(deltaSeconds, *this))
        {
            RequestQuit(*exitCode);
        }

        // The mode decides what the keys mean (M29).
        const InputContextId context = m_mode == Mode::InDialogue ? InputContextId::Dialogue
            : m_mode == Mode::AtMachine ? InputContextId::Machine
            : m_mode == Mode::Viewing ? InputContextId::Viewer
            : m_mode == Mode::Driving ? InputContextId::Driving
            : InputContextId::Exploring;
        m_actions.Update(m_inputMap, context, GetInput());

        m_time += deltaSeconds;
        m_messages.Update(deltaSeconds);
        if (m_simulateLossAt && m_time >= *m_simulateLossAt)
        {
            GetRenderer().SimulateSwapchainLoss(); // M63: the next frame fails as a lost swapchain does
            m_simulateLossAt.reset();
        }

        m_levels->Update(deltaSeconds);
        if (m_calibration.active)
        {
            UpdateCalibration(static_cast<float>(m_diagnostics.RealFrameMs() / 1000.0)); // M64: real time, not a fixed step
        }
        GetRenderer().SetFade(m_levels->GetFade());
        // ATOM_LATENCY_FLASH=1 (M72): a left click turns this one frame
        // black - an unmistakable effect of the input, for latency
        // measurement (Tools/Perf/latency.ps1). Inert unless set.
        if (m_latencyFlash && GetInput().WasLeftClicked())
        {
            GetRenderer().SetFade(1.0f);
        }
        if (m_environment.IsTransitioning())
        {
            m_environment.Update(deltaSeconds); // game time: fixed steps blend the same way
            ApplyLighting();
        }
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
            m_mode = RestingMode();
        }
        if (m_mode != Mode::Transitioning)
        {
            m_arriving = false; // the player may move from here on
        }

        switch (m_mode)
        {
        case Mode::Exploring:
        {
            // The demo's input map, read here; the controller takes plain values.
            PlayerController::MoveIntent intent;
            intent.move.y = (m_actions.Held(InputAction::MoveForward) ? 1.0f : 0.0f) - (m_actions.Held(InputAction::MoveBack) ? 1.0f : 0.0f);
            intent.move.x = (m_actions.Held(InputAction::MoveRight) ? 1.0f : 0.0f) - (m_actions.Held(InputAction::MoveLeft) ? 1.0f : 0.0f);
            intent.jog = m_actions.Held(InputAction::Jog);
            m_player.Update(GetInput(), intent, m_camera, CurrentCollision(), deltaSeconds);
        }
            UpdateInteraction();
            break;
        case Mode::InDialogue:
            UpdateDialogue(deltaSeconds);
            break;
        case Mode::InSequence:
            m_target = {};
            UpdateSequence(deltaSeconds);
            break;
        case Mode::AtMachine:
            m_target = {};
            UpdateMachine(deltaSeconds);
            break;
        // Tab switches between the lab's two modes, once per press: the
        // mode entered this frame doesn't see the same press again.
        case Mode::Viewing:
            m_target = {};
            if (m_actions.Pressed(InputAction::ToggleDrive))
            {
                BeginDrive();
            }
            else
            {
                UpdateLab(deltaSeconds);
            }
            break;
        case Mode::Driving:
            m_target = {};
            if (m_actions.Pressed(InputAction::ToggleDrive))
            {
                EndDrive();
            }
            else
            {
                UpdateDrive(deltaSeconds);
            }
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
        // Footsteps: the player's, or in drive mode the character's, timed
        // by its animation's foot-down events.
        const bool driving = m_mode == Mode::Driving;
        m_audioScape.Update(deltaSeconds, m_camera, AudioScape::Listener{
            driving ? m_driveBody.GetFeetPosition() : m_player.GetFeetPosition(),
            driving ? m_driveSteps : m_player.GetStepCount(),
            driving ? m_driveBody.IsGrounded() : m_player.IsGrounded(),
            input.IsKeyDown(SDL_SCANCODE_LSHIFT)
        });

        Atom::Renderer& renderer = GetRenderer();
        renderer.SetCamera(
            m_camera.GetViewMatrix(),
            m_camera.verticalFov,
            m_camera.nearPlane,
            m_camera.farPlane
        );

        Level* drawn = m_drawWorld ? m_levels->GetLevel() : nullptr;
        if (drawn)
        {
            drawn->Submit(renderer, m_player.GetFeetPosition());
        }
        UpdateFlashlight(deltaSeconds);
        if (m_flashlight.IsOn() && !m_devSpotOn)
        {
            renderer.SubmitSpotLight(m_flashlight.GetLight());
        }
        if (m_devSpotOn)
        {
            if (m_devSpotFollows)
            {
                // Held low and to the right, as a hand would hold it.
                const glm::vec3 right = m_camera.GetFlatRight();
                m_devSpot.position = m_camera.GetPosition() + right * 0.18f + glm::vec3{ 0.0f, -0.15f, 0.0f };
                m_devSpot.direction = m_camera.GetForward();
            }
            renderer.SubmitSpotLight(m_devSpot);
        }

        // Weather (M50): the environment's wind and, outdoors, its rain.
        const Atom::SceneLighting& lighting = renderer.GetLighting();
        m_atmosphere.SetWind(m_environment.Current().wind);
        m_atmosphere.SetRain(lighting.rain, lighting.skyColor * 0.55f + glm::vec3{ 0.18f });
        m_audioScape.SetRainLevel(lighting.rain);

        // Fog banks stay faintly visible with fog off: morning haze.
        m_atmosphere.Update(
            deltaSeconds,
            m_player.GetFeetPosition(),
            lighting.fogColor,
            lighting.fogDensity > 0.0f ? 1.0f : 0.35f
        );
        if (m_drawWorld)
        {
            m_atmosphere.Submit(renderer);
        }
        // Sway follows the gusts outdoors; indoors the air is still.
        const Level* current = m_levels->GetLevel();
        renderer.SetWind(current && current->GetData().outdoor ? m_atmosphere.GetWind() : glm::vec3{ 0.0f }, m_time);

        m_unease.Update(deltaSeconds, m_camera, m_player.GetFeetPosition(), m_audioScape);
        if (m_drawWorld)
        {
            m_unease.Submit(renderer);
        }

        DrawOverlay(deltaSeconds);
        DrawMachineView(); // over everything: the machine fills the window
        UpdateWindowTitle(deltaSeconds);
        DrawDevTools(deltaSeconds);
    }

    void DemoApp::UpdateFlashlight(float deltaSeconds)
    {
        m_flashlight.SetOwned(m_gameState.HasFlag(FlashlightFlag));
        ApplyGoneEntities();
        // F works wherever you walk (not in dialogue or at the machine).
        if (m_mode == Mode::Exploring && m_actions.Pressed(InputAction::ToggleLight) && m_flashlight.Toggle())
        {
            Atom::PlayParams click{};
            click.gain = 0.6f;
            GetAudio().Play(m_audioScape.GetSound("switch_click"), click);
        }
        // Arriving somewhere, the beam is already where you look.
        m_flashlight.Update(m_camera.GetPosition(), m_camera.GetForward(), m_camera.GetFlatRight(),
                            deltaSeconds, m_arriving);
    }

    InteractionSystem::Settings DemoApp::TargetSettings() const
    {
        InteractionSystem::Settings settings;
        const Atom::CollisionWorld* collision = const_cast<DemoApp*>(this)->CurrentCollision();
        settings.isLit = [this, collision](const glm::vec3& point) { return m_flashlight.Lights(point, collision); };
        return settings;
    }

    void DemoApp::ApplyGoneEntities()
    {
        GameWorld* world = CurrentWorld();
        if (!world)
        {
            return;
        }
        world->ForEach([&](EntityId, Entity& entity) {
            if (!entity.goneWithFlag.empty() && m_gameState.HasFlag(entity.goneWithFlag))
            {
                entity.hidden = true;
                entity.interactable.reset();
            }
        });
    }

    bool DemoApp::IsLit(const std::string& name) const
    {
        const Entity* entity = const_cast<DemoApp*>(this)->FindEntity(name);
        if (!entity)
        {
            return false;
        }
        const glm::vec3 focus = entity->position + (entity->interactable ? entity->interactable->focusOffset : glm::vec3{ 0.0f });
        return m_flashlight.Lights(focus, const_cast<DemoApp*>(this)->CurrentCollision());
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
            FogPresets[m_view.fogPreset].name,
            GetRenderer().GetLighting().shadowsEnabled ? "on" : m_view.shadows ? "off (level)" : "off",
            m_view.postMode == 0 ? "full" : m_view.postMode == 1 ? "grade" : "off",
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
        // The lab has its own help line (DrawLabOverlay).
        const float hintAlpha = m_showHud && !m_lab ? std::clamp((9.0f - m_hintTime) / 1.5f, 0.0f, 1.0f) : 0.0f;
        if (hintAlpha > 0.0f)
        {
            const char* hint = m_flashlight.IsOwned()
                ? "WASD move   Shift jog   Mouse look   E interact   F light   F1 debug"
                : "WASD move   Shift jog   Mouse look   E interact   F1 debug";
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
        else if (m_mode == Mode::Viewing || m_mode == Mode::Driving)
        {
            DrawLabOverlay(scale);
        }
        else if (m_mode == Mode::Exploring)
        {
            DrawInteractionPrompt(scale);
            if (hintAlpha <= 0.0f)
            {
                m_messages.Draw(ui, *m_font, scale * 0.85f);
            }
        }

        // Frame time, smoothed so the numbers are readable (the title bar).
        const float frameMs = deltaSeconds * 1000.0f;
        m_smoothedFrameMs += (frameMs - m_smoothedFrameMs) * 0.05f;

        // M82: F1 is the engine's shared overlay; the demo appends its own
        // lines after the engine's (frame, scene, draws, device).
        Atom::DevTools& tools = GetDevTools();
        if (!tools.IsOverlayVisible())
        {
            return;
        }
        const Atom::FrameStats& stats = GetRenderer().GetLastFrameStats();
        const Atom::RenderSettings& settings = GetRenderer().GetSettings();
        const glm::vec3& feet = m_player.GetFeetPosition();
        char line[256];

        // Per distance layer (M22): chunks in view, draw calls, triangles,
        // and shadow-pass draws - mid and far should show none.
        const char* layerNames[] = { "Near", "Mid ", "Far " };
        for (std::size_t i = 0; i < Atom::RenderLayerCount; ++i)
        {
            const Atom::LayerStats& l = stats.layers[i];
            std::snprintf(line, sizeof(line), "%s  chunks %u/%u  draws %u  tris %.1fk  shadow %u", layerNames[i],
                          l.chunksVisible, l.chunks, l.drawn, l.triangles / 1000.0f, l.shadowDrawn);
            tools.AddOverlayLine(line);
        }
        std::snprintf(line, sizeof(line), "Render scale %.0f%%   models loaded %zu, shared %zu", settings.renderScale * 100.0f,
                      m_modelCache.GetLoads(), m_modelCache.GetHits());
        tools.AddOverlayLine(line);
        std::snprintf(line, sizeof(line), "Fog %s   Shadows %s   Baked light %s   Post %s", FogPresets[m_view.fogPreset].name,
                      GetRenderer().GetLighting().shadowsEnabled ? "on" : m_view.shadows ? "off (level)" : "off",
                      m_view.bakedLight ? "on" : "off", m_view.postMode == 0 ? "full" : m_view.postMode == 1 ? "grade" : "off");
        tools.AddOverlayLine(line);
        std::snprintf(line, sizeof(line), "Particles %s   Unease %s   Audio %s", m_atmosphere.IsEnabled() ? "on" : "off",
                      m_unease.IsEnabled() ? "on" : "off", m_audioScape.IsMuted() ? "muted" : "on");
        tools.AddOverlayLine(line);
        std::snprintf(line, sizeof(line), "Position %.1f  %.2f  %.1f", feet.x, feet.y, feet.z);
        tools.AddOverlayLine(line);
        std::snprintf(line, sizeof(line), "Level %s   voices %zu   flags %zu",
                      m_levels->GetLevel() ? m_levels->GetLevel()->GetName().c_str() : "-", GetAudio().GetVoiceCount(),
                      m_gameState.FlagCount());
        tools.AddOverlayLine(line);
        (void)ui;
        (void)scale;
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

        if (m_actions.Pressed(InputAction::ChoiceUp))
        {
            m_dialogue.MoveSelection(-1);
        }
        if (m_actions.Pressed(InputAction::ChoiceDown))
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
        if (m_actions.Pressed(InputAction::Confirm))
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
            m_mode = RestingMode();
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
        if ((!input.IsMouseCaptured() && !m_diagnostics.HasTestScript()) || !world)
        {
            m_target = {};
            return;
        }

        m_target = InteractionSystem::FindTarget(
            *world, CurrentCollision(), m_camera.GetPosition(), m_camera.GetForward(), TargetSettings());

        const Entity* target = world->Find(m_target);
        if (target && m_actions.Pressed(InputAction::Interact))
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
            [this](const PlayMachine& play) { return BeginMachine(play); },
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
        const std::string prompt = "[E]  " + InteractionSystem::ResolvePrompt(*target->interactable, m_gameState);
        const float textScale = scale * 0.9f;
        const glm::vec2 size = ui.MeasureText(*m_font, prompt, textScale);
        const glm::vec2 position{ (screen.x - size.x) * 0.5f, screen.y * 0.62f };

        ui.DrawText(*m_font, prompt, position + glm::vec2{ 2.0f * scale },
            { 0.0f, 0.0f, 0.0f, 0.6f }, textScale);
        ui.DrawText(*m_font, prompt, position, { 0.95f, 0.93f, 0.86f, 1.0f }, textScale);
    }

    void DemoApp::UpdateRenderSettings()
    {
        // F2-F8 are the framework's view toggles (v0.0.14, shared with the
        // Showcase); F9 is the demo's own.
        if (HandleViewKeys(GetInput(), GetRenderer(), m_view, &m_atmosphere))
        {
            ApplyLighting();
        }

        // F9: unease events (figure, static, flicker) on/off.
        if (GetInput().WasKeyPressed(SDL_SCANCODE_F9))
        {
            m_unease.SetEnabled(!m_unease.IsEnabled());
        }
    }

    void DemoApp::ApplyLighting()
    {
        // The level's light under the current environment, the viewer's
        // toggles on top (the framework's mapping since v0.0.14).
        const Level* level = m_levels ? m_levels->GetLevel() : nullptr;
        GetRenderer().SetLighting(SceneLightingFor(level, m_environment.Current(), m_view));
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
        else if (!input.IsMouseCaptured() && !GetDevTools().IsVisible()
            && (SDL_GetMouseState(nullptr, nullptr) & SDL_BUTTON_LMASK))
        {
            input.SetMouseCaptured(GetWindow().GetSDLWindow(), true);
        }
    }

    // --- Scripted tests ------------------------------------------------------

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
        if (m_mode == Mode::Driving)
        {
            // Drive mode: the character moves, and the camera looks the
            // given way (forward walks along it).
            m_driveBody.Place(feet);
            m_arm.Reset(-yawDegrees, m_arm.GetPitchDegrees());
            return;
        }
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
            *world, CurrentCollision(), m_camera.GetPosition(), m_camera.GetForward(), TargetSettings());
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
            *world, CurrentCollision(), m_camera.GetPosition(), m_camera.GetForward(), TargetSettings());
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
        case Mode::AtMachine: return "machine";
        case Mode::Viewing: return "viewer";
        case Mode::Driving: return "drive";
        default: return "exploring";
        }
    }

    std::string DemoApp::Message() const
    {
        return m_messages.GetText();
    }

    std::string DemoApp::DialogueNodeId() const
    {
        const Demo::DialogueNode* node = m_dialogue.GetNode();
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

    std::pair<std::uint32_t, std::uint32_t> DemoApp::ScreenStats() const
    {
        const Atom::FrameStats& stats = const_cast<DemoApp*>(this)->GetRenderer().GetLastFrameStats();
        return { stats.renderTextures, stats.renderTextureDraws };
    }

    bool DemoApp::BeginMachine(const PlayMachine& play)
    {
        Level* level = m_levels->GetLevel();
        Atom::RenderTexture* screen = level && m_mode == Mode::Exploring ? level->TakeOverScreen(play.screen) : nullptr;
        if (!screen)
        {
            return false;
        }
        const PlayfieldParseResult field = LoadPlayfieldFile(m_assets.Resolve(play.machine));
        if (!field.playfield)
        {
            std::cerr << field.error << '\n';
            level->ReleaseScreen(play.screen);
            return false;
        }
        m_machinePlay = play;
        m_machineScreen = screen;
        m_machineClock = FixedStep{};
        m_machineGame.emplace(*field.playfield, ++m_machineSessions);
        m_machineGame->AddToTray(m_gameState.GetCounter("balls"));
        level->SetGroupGain("bed", 0.35f); // the hall goes quieter as you lean in
        const CameraPose from{ m_camera.GetPosition(), m_camera.GetYaw(), m_camera.GetPitch() };
        const CameraPose to{ play.viewPosition, glm::radians(play.viewYawDegrees), glm::radians(play.viewPitchDegrees) };
        m_machine.Enter(from, to);
        m_mode = Mode::AtMachine;
        std::cout << "Sat down at the machine\n";
        return true;
    }

    void DemoApp::UpdateMachine(float deltaSeconds)
    {
        m_machine.Update(deltaSeconds);
        if (m_machine.GetPhase() == MachineMode::Phase::Playing && m_actions.Pressed(InputAction::Leave))
        {
            m_machine.Leave();
        }
        const CameraPose pose = m_machine.GetCamera();
        m_camera.SetPosition(pose.position);
        m_camera.SetRotation(pose.yaw, pose.pitch);

        // The game runs on its own fixed clock and draws into the screen. The
        // handle and the knob only work while seated, not during the move.
        const bool playing = m_machine.GetPhase() == MachineMode::Phase::Playing;
        // Buying: tokens for balls, straight into the tray.
        if (playing && m_machineGame && m_actions.Pressed(InputAction::Buy))
        {
            if (m_gameState.Spend("tokens", TokensPerBuy))
            {
                m_machineGame->AddToTray(BallsPerBuy);
                Atom::PlayParams params{};
                params.gain = 0.5f;
                GetAudio().Play(m_audioScape.GetSound("payout"), params);
            }
        }
        float wheel = playing ? GetInput().GetWheelDelta() * 0.05f : 0.0f;
        const float knob = playing ? (m_actions.Held(InputAction::StrengthUp) ? 0.5f : 0.0f)
                                   - (m_actions.Held(InputAction::StrengthDown) ? 0.5f : 0.0f) : 0.0f;
        for (int steps = m_machineClock.Advance(deltaSeconds); steps > 0 && m_machineGame; --steps)
        {
            m_machineGame->Step({ playing && m_actions.Held(InputAction::Launch), knob * PachinkoGame::Tick + wheel });
            wheel = 0.0f; // a wheel notch turns the knob once
            PlayMachineSounds(*m_machineGame);
        }
        if (m_machineScreen && m_machineGame)
        {
            Atom::UIRenderer& canvas = m_machineScreen->GetCanvas();
            m_machineGame->Draw(canvas);
            // Tokens under the tray count, in green.
            canvas.DrawRect({ 4.0f, 40.0f }, { 42.0f, 2.0f }, { 0.2f, 0.5f, 0.3f, 1.0f });
            DrawNumber(canvas, static_cast<std::uint32_t>(m_gameState.GetCounter("tokens")), 4, { 6.0f, 46.0f },
                       { 7.0f, 14.0f }, 2.0f, 3.0f, { 0.45f, 1.0f, 0.55f, 1.0f });
        }
        if (!m_machine.IsActive())
        {
            EndMachine();
        }
    }

    void DemoApp::EndMachine()
    {
        if (Level* level = m_levels->GetLevel())
        {
            level->ReleaseScreen(m_machinePlay.screen);
            level->SetGroupGain("bed", 1.0f);
        }
        m_machineScreen = nullptr;
        if (m_machineGame)
        {
            m_gameState.SetCounter("balls", m_machineGame->GetTray()); // balls on the board are lost
            m_machineGame.reset();
        }
        m_mode = Mode::Exploring; // the player's eye and look are where they were
        std::cout << "Stood up from the machine\n";
    }

    void DemoApp::PlayMachineSounds(const PachinkoGame& game)
    {
        Atom::AudioSystem& audio = GetAudio();
        const auto play = [&](std::string_view name, float gain, float pitch = 1.0f) {
            Atom::PlayParams params{};
            params.gain = gain;
            params.pitch = pitch;
            audio.Play(m_audioScape.GetSound(name), params);
        };
        // Balls on nails: the loudest few per tick, pitched by what they hit.
        int clicks = 0;
        for (const Impact& impact : game.GetImpacts())
        {
            if (++clicks > 3)
            {
                break;
            }
            const float pitch = impact.kind == Impact::Kind::Nail ? 1.0f : impact.kind == Impact::Kind::Ball ? 1.3f : 0.7f;
            play("ball_click", std::min(0.25f, impact.speed / 1600.0f), pitch * (0.95f + 0.1f * (impact.ball % 7) / 7.0f));
        }
        for (const PocketEvent& event : game.GetEvents())
        {
            if (event.kind == Pocket::Kind::Start)
            {
                play("pocket_chime", 0.4f);
            }
            if (event.paid > 1)
            {
                play("payout", std::min(0.5f, 0.15f + event.paid * 0.02f));
            }
        }
        for (const PachinkoRules::Event event : game.GetRules().GetEvents())
        {
            switch (event)
            {
            case PachinkoRules::Event::ReelStop: play("reel_stop", 0.35f); break;
            case PachinkoRules::Event::Reach: play("reach", 0.45f); break;
            case PachinkoRules::Event::Hit: play("fanfare", 0.6f); break;
            case PachinkoRules::Event::RoundStart: play("pocket_chime", 0.5f, 0.75f); break;
            default: break;
            }
        }
    }

    void DemoApp::DrawMachineView()
    {
        const float fade = m_machine.GetFade();
        if (!m_machine.IsActive() || fade <= 0.0f || !m_machineScreen)
        {
            return;
        }
        Atom::UIRenderer& ui = GetRenderer().GetUI();
        const glm::vec2 window = ui.GetScreenSize();
        const PixelLayout layout = FitIntegerScale(window, static_cast<int>(m_machineScreen->GetWidth()),
                                                   static_cast<int>(m_machineScreen->GetHeight()));
        ui.DrawRect({ 0.0f, 0.0f }, window, { 0.02f, 0.02f, 0.03f, fade });
        ui.DrawImage(m_machineScreen->GetTexture(), layout.position, layout.size, { 1.0f, 1.0f, 1.0f, fade });
        if (m_font && m_machine.GetPhase() == MachineMode::Phase::Playing)
        {
            const float scale = std::max(1.0f, window.y / 1080.0f);
            ui.DrawText(*m_smallFont, "Space  launch\nUp/Down  strength\nB  buy 50 balls (10 tokens)\nQ  leave",
                        { 24.0f * scale, window.y - 112.0f * scale }, { 0.8f, 0.78f, 0.72f, 0.8f * fade }, scale);
        }
    }

    bool DemoApp::HoldAction(const std::string& name, bool held)
    {
        const std::optional<InputAction> action = ActionFromName(name);
        if (action)
        {
            m_actions.Inject(*action, held);
        }
        return action.has_value();
    }

    bool DemoApp::PressAction(const std::string& name)
    {
        const std::optional<InputAction> action = ActionFromName(name);
        if (action)
        {
            m_actions.InjectPress(*action);
        }
        return action.has_value();
    }

    std::string DemoApp::Capture(const std::string& stem, bool includeUi)
    {
        const std::string path = m_outputRoot + "out/img/" + stem + ".png";
        GetRenderer().RequestCapture(path, includeUi);
        return path;
    }

    bool DemoApp::CapturePending() const
    {
        return const_cast<DemoApp*>(this)->GetRenderer().IsCapturePending();
    }

    bool DemoApp::Set(const std::string& what, const std::string& value)
    {
        Atom::Renderer& renderer = GetRenderer();
        Atom::RenderSettings settings = renderer.GetSettings();
        const bool on = value == "on";
        const bool onOff = on || value == "off";
        float number = 0.0f;
        const bool isNumber = std::from_chars(value.data(), value.data() + value.size(), number).ec == std::errc{};

        if (what == "msaa" && (value == "1" || value == "2" || value == "4"))
        {
            settings.msaaSamples = static_cast<std::uint32_t>(number);
        }
        else if (what == "scale" && isNumber && number >= 0.1f && number <= 1.0f)
        {
            settings.renderScale = number;
        }
        else if (what == "post" && (value == "full" || value == "grade" || value == "off"))
        {
            m_view.postMode = value == "full" ? 0 : value == "grade" ? 1 : 2;
            settings.post = Atom::PostSettings{};
            settings.post.enabled = m_view.postMode != 2;
            if (m_view.postMode == 1)
            {
                settings.post.grain = 0.0f;
                settings.post.vignette = 0.0f;
            }
        }
        else if (what == "quality")
        {
            // M61: a quality mode for this run (never saved from a script).
            const std::optional<QualityMode> mode = ParseQualityMode(value);
            if (!mode)
            {
                return false;
            }
            SetQualityMode(*mode, false);
            return true;
        }
        else if (what == "fog")
        {
            const auto found = std::find_if(std::begin(FogPresets), std::end(FogPresets),
                [&](const FogPreset& preset) { return value == preset.name; });
            if (found == std::end(FogPresets))
            {
                return false;
            }
            m_view.fogPreset = static_cast<std::size_t>(found - std::begin(FogPresets));
        }
        else if (what == "shadows" && onOff) m_view.shadows = on;
        else if (what == "sun" && onOff) m_view.sun = on;
        else if (what == "particles" && onOff) m_atmosphere.SetEnabled(on);
        else if (what == "water" && onOff) renderer.SetWaterEnabled(on); // M51: for benchmarks
        else if (what == "reflection" && onOff) renderer.SetReflectionEnabled(on); // M51: to measure it
        else if (what == "rain" && isNumber && number >= 0.0f && number <= 1.0f) m_view.rain = number;
        else if (what == "rain" && value == "level") m_view.rain.reset(); // the environment's again
        else if (what == "weather" && onOff)
        {
            // M51: the lake's weather all at once - water and rain - or none.
            renderer.SetWaterEnabled(on);
            m_view.rain = on ? 1.0f : 0.0f;
        }
        else if (what == "unease" && onOff) m_unease.SetEnabled(on);
        else if (what == "world" && onOff) m_drawWorld = on;
        else if (what == "hud" && onOff) m_showHud = on;
        else if (what == "overlay" && onOff) GetDevTools().SetOverlayVisible(on); // M82: the shared F1 overlay
        else if (what == "devtools" && onOff) GetDevTools().SetVisible(on); // F10 (M41)
        else if (what == "devtools_collapsed" && onOff) m_devToolsCollapse = on; // every panel, next frame
        else if (what == "spot" && onOff) m_devSpotOn = on; // M42: the test spot, at the camera
        else if (what == "spot_follow" && onOff) m_devSpotFollows = on; // off: it stays where it is
        else if (what == "spot_shadows" && onOff) m_devSpot.castsShadows = on; // M43
        else if (what == "flashlight" && onOff) // M44: found and switched on (or off)
        {
            if (on)
            {
                m_gameState.SetFlag(FlashlightFlag);
                m_flashlight.SetOwned(true);
            }
            m_flashlight.SetOn(on);
        }
        else if (what == "spot_offset" && isNumber && number >= 0.0f && number <= 0.1f) m_devSpot.shadowNormalOffset = number;
        else if (what == "mode" && m_lab && (value == "clips" || value == "blend" || value == "machine"))
        {
            m_viewer.SelectMode(value == "clips" ? ViewerMode::Clips
                : value == "blend" ? ViewerMode::Blend : ViewerMode::StateMachine);
            ApplyLabPose();
        }
        else if (what == "blend" && m_lab && isNumber && number >= 0.0f && number <= 1.0f)
        {
            m_viewer.SetBlendWeight(number);
            ApplyLabPose();
        }
        else if ((what == "skeleton" || what == "weights" || what == "bind" || what == "pause") && onOff && m_lab)
        {
            if (what == "skeleton") m_viewer.SetSkeleton(on);
            else if (what == "weights") m_viewer.SetWeights(on);
            else if (what == "bind") m_viewer.SetBindPose(on);
            else m_viewer.SetPaused(on);
            ApplyLabPose();
        }
        else if (what == "fov" && isNumber && number >= 10.0f && number <= 150.0f)
        {
            m_camera.verticalFov = glm::radians(number);
        }
        else if (what == "fixed_dt" && isNumber && number >= 0.0f && number <= 0.25f)
        {
            m_diagnostics.SetFixedStep(number);
        }
        else
        {
            return false;
        }
        renderer.SetSettings(settings);
        ApplyLighting();
        return true;
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
