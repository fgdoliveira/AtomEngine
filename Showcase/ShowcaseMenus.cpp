// The Showcase's menus and benchmark (v0.0.14, M90).
//
// The title is a menu over the living village (the clock runs behind it):
// Explore walks it, Benchmark flies a fixed path through it and writes the
// frame times to a file, Settings is the framework's player settings
// screen, Quit. Esc while exploring brings the same menu back as a pause.
#include "ShowcaseApp.h"

#include "Core/DevSwitch.h"
#include "Platform/Input.h"
#include "Platform/Window.h"
#include "Renderer/Renderer.h"

#include <SDL3/SDL.h>

#include <glm/common.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/trigonometric.hpp>

#include <algorithm>
#include <cmath>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace Showcase
{
    namespace
    {
        enum MenuItem { Explore, Benchmark, Settings, Quit };

        // The benchmark's flight: eye, yaw and pitch (degrees; yaw 0 looks
        // down -Z, toward the lake), visited at equal times. Over the jetty,
        // along the lane, past the workshop, through the shrine's gate, over
        // the shore, to the radio shed.
        struct Waypoint
        {
            glm::vec3 eye;
            float yaw;
            float pitch;
        };
        constexpr Waypoint Flight[] = {
            { { 0.0f, 3.0f, -20.0f }, 180.0f, -5.0f },
            { { 0.0f, 4.0f, -5.0f }, 180.0f, -8.0f },
            { { -10.0f, 3.0f, 1.0f }, -90.0f, -5.0f },
            { { -22.0f, 2.5f, 0.0f }, 167.0f, -4.0f },
            { { -25.5f, 2.0f, -3.0f }, 0.0f, 0.0f },
            { { -20.0f, 6.0f, -18.0f }, 70.0f, -10.0f },
            { { 10.0f, 5.0f, -2.0f }, 90.0f, -8.0f },
            { { 24.0f, 2.5f, -1.0f }, 16.0f, -3.0f },
        };
        constexpr int Legs = static_cast<int>(std::size(Flight)) - 1;
        // The times of day it flies through, a quarter each.
        constexpr const char* Phases[] = { "clear_day", "sunset", "night", "rain" };
        constexpr float WarmUpSeconds = 0.5f; // after each switch: not measured

        float Smooth(float t)
        {
            return t * t * (3.0f - 2.0f * t);
        }

        std::string Stamp(const char* format)
        {
            const std::time_t now = std::time(nullptr);
            std::tm local{};
#ifdef _WIN32
            localtime_s(&local, &now);
#else
            localtime_r(&now, &local);
#endif
            char text[64];
            std::strftime(text, sizeof(text), format, &local);
            return text;
        }
    }

    Atom::Application::StartupConfig ShowcaseApp::OnConfigure()
    {
        // Saved settings (M90, the framework's store) - never for a scripted
        // run, which behaves the same on every machine. The GPU choice must
        // be known before the device exists.
        const bool scripted = Atom::DevSwitch("ATOM_TEST_SCRIPT") != nullptr;
        m_store.Open("Showcase", !scripted, false);
        m_resolved = ResolveSettings({}, {}, scripted ? nullptr : &m_store.Settings());
        StartupConfig config;
        config.gpuPreference = m_resolved.gpu == GpuPreference::HighPerformance ? Atom::GPUPreference::HighPerformance
                                                                                 : Atom::GPUPreference::LowPower;
        return config;
    }

    void ShowcaseApp::ApplySettings()
    {
        const GameSettings& s = m_store.Settings();
        ApplyDisplayAndVolume(GetWindow().GetSDLWindow(), &GetAudio(), m_audio.get(), s);
        ApplyQualityTier(GetRenderer(), m_view, &m_atmosphere,
                         TierFor(s.quality, s.calibration, GetRenderer().GetAdapterName()));
        ApplyLighting();
    }

    void ShowcaseApp::OpenMenu()
    {
        m_menu = MenuScreen("AtomEngine", m_benchmarkResult.empty()
                                              ? "A lakeside village through a day - the engine's showcase"
                                              : m_benchmarkResult,
                            { m_explored ? "Resume" : "Explore", "Benchmark", "Settings", "Quit" });
        m_screen = Screen::Menu;
        m_lens = false;
        // Keyboard-only: the pointer stays captured, so no cursor shows (M90).
        GetInput().SetMouseCaptured(GetWindow().GetSDLWindow(), true);
    }

    void ShowcaseApp::UpdateScreens(float deltaSeconds)
    {
        Atom::Input& input = GetInput();
        const bool escape = input.WasKeyPressed(SDL_SCANCODE_ESCAPE);
        switch (m_screen)
        {
        case Screen::None:
            // Esc: the lab closes first; walking, the menu opens.
            if (escape && m_mode == Mode::Walking)
            {
                OpenMenu();
            }
            break;
        case Screen::Menu:
            if (escape && m_explored)
            {
                m_screen = Screen::None; // back to walking
                input.SetMouseCaptured(GetWindow().GetSDLWindow(), true);
                break;
            }
            if (const std::optional<int> chosen = m_menu.Update(input))
            {
                switch (*chosen)
                {
                case Explore:
                    m_explored = true;
                    m_screen = Screen::None;
                    input.SetMouseCaptured(GetWindow().GetSDLWindow(), true);
                    break;
                case Benchmark:
                    StartBenchmark();
                    break;
                case Settings:
                    m_settingsScreen = SettingsScreen();
                    m_screen = Screen::Settings;
                    break;
                case Quit:
                    RequestQuit();
                    break;
                }
            }
            break;
        case Screen::Settings:
        {
            const SettingsScreen::Result result = m_settingsScreen.Update(input, m_store.Settings());
            if (result.changed)
            {
                ApplySettings();
                m_store.Save();
            }
            if (result.back)
            {
                OpenMenu();
            }
            break;
        }
        case Screen::Benchmark:
            if (escape)
            {
                m_benchmarkResult = "Benchmark stopped";
                ResetEnvironment();
                ApplyLighting();
                OpenMenu();
                break;
            }
            UpdateBenchmark(deltaSeconds);
            break;
        }
    }

    void ShowcaseApp::StartBenchmark()
    {
        // Fixed: the path, the times of day and the clock (stopped). The
        // lens and the HUD are off; the player's quality is what's measured.
        m_screen = Screen::Benchmark;
        m_benchmarkTime = 0.0f;
        m_benchmarkFrames.Clear();
        for (Atom::FrameStatsWindow& phase : m_benchmarkPhases)
        {
            phase.Clear();
        }
        m_clockOn = false;
        m_lens = false;
        SetEnvironment(Phases[0], 0.0f);
        std::cout << "Benchmark: " << m_benchmarkSeconds << " s over the village\n";
    }

    void ShowcaseApp::UpdateBenchmark(float deltaSeconds)
    {
        const float phaseSeconds = m_benchmarkSeconds / static_cast<float>(std::size(Phases));
        const int phase = std::min(static_cast<int>(m_benchmarkTime / phaseSeconds), static_cast<int>(std::size(Phases)) - 1);
        const float intoPhase = m_benchmarkTime - phaseSeconds * static_cast<float>(phase);
        if (m_environmentName != Phases[phase])
        {
            SetEnvironment(Phases[phase], 0.0f);
        }
        // The real frame (not the stepped one a scenario may fix).
        if (intoPhase > WarmUpSeconds)
        {
            const double ms = m_diagnostics.RealFrameMs();
            m_benchmarkFrames.AddSample(ms);
            m_benchmarkPhases[phase].AddSample(ms);
        }

        // The camera along the path: each leg eased in and out.
        const float t = std::clamp(m_benchmarkTime / m_benchmarkSeconds, 0.0f, 1.0f) * static_cast<float>(Legs);
        const int leg = std::min(static_cast<int>(t), Legs - 1);
        const float u = Smooth(t - static_cast<float>(leg));
        const Waypoint& a = Flight[leg];
        const Waypoint& b = Flight[leg + 1];
        const float yaw = a.yaw + std::remainder(b.yaw - a.yaw, 360.0f) * u; // the short way round
        m_camera.SetPosition(glm::mix(a.eye, b.eye, u));
        m_camera.SetRotation(glm::radians(yaw), glm::radians(glm::mix(a.pitch, b.pitch, u)));

        m_benchmarkTime += deltaSeconds;
        if (m_benchmarkTime >= m_benchmarkSeconds)
        {
            FinishBenchmark();
        }
    }

    void ShowcaseApp::FinishBenchmark()
    {
        // Stable fields, one per line ("key: value"), so runs compare with a diff.
        Atom::Renderer& renderer = GetRenderer();
        const Atom::FrameStats& stats = renderer.GetLastFrameStats();
        const Atom::RenderSettings& settings = renderer.GetSettings();
        const glm::vec2 window = renderer.GetUI().GetScreenSize();
        const GameSettings& s = m_store.Settings();
        const auto ms = [](double value) {
            char text[32];
            std::snprintf(text, sizeof(text), "%.3f", value);
            return std::string(text);
        };
        std::string report;
        report += "benchmark: AtomEngine Showcase\n";
        report += "version: " ATOM_VERSION "\n";
        report += "date: " + Stamp("%Y-%m-%d %H:%M:%S") + "\n";
        report += "adapter: " + renderer.GetAdapterName() + "\n";
        report += "backend: " + renderer.GetBackendName() + "\n";
        report += "window: " + std::to_string(static_cast<int>(window.x)) + "x" + std::to_string(static_cast<int>(window.y)) + "\n";
        report += "scene: " + std::to_string(stats.sceneWidth) + "x" + std::to_string(stats.sceneHeight) + "\n";
        report += "msaa: " + std::to_string(settings.msaaSamples) + "x\n";
        report += "render_scale: " + ms(settings.renderScale) + "\n";
        report += "quality: " + std::string(ToString(s.quality)) + "\n";
        report += "seconds: " + ms(m_benchmarkSeconds) + "\n";
        report += "frames: " + std::to_string(m_benchmarkFrames.Count()) + "\n";
        report += "median_ms: " + ms(m_benchmarkFrames.Median()) + "\n";
        report += "p95_ms: " + ms(m_benchmarkFrames.Percentile(0.95)) + "\n";
        report += "p99_ms: " + ms(m_benchmarkFrames.Percentile(0.99)) + "\n";
        report += "worst_ms: " + ms(m_benchmarkFrames.Percentile(1.0)) + "\n";
        report += "mean_ms: " + ms(m_benchmarkFrames.Mean()) + "\n";
        for (std::size_t i = 0; i < std::size(Phases); ++i)
        {
            report += std::string(Phases[i]) + "_median_ms: " + ms(m_benchmarkPhases[i].Median()) + "\n";
            report += std::string(Phases[i]) + "_p95_ms: " + ms(m_benchmarkPhases[i].Percentile(0.95)) + "\n";
        }

        const std::filesystem::path folder = std::filesystem::path(m_outputRoot) / "out";
        std::error_code error;
        std::filesystem::create_directories(folder, error);
        const std::filesystem::path file = folder / ("benchmark-" + Stamp("%Y%m%d-%H%M%S") + ".txt");
        std::ofstream(file, std::ios::binary) << report;
        std::cout << report << "Benchmark written to " << file.string() << '\n';

        char summary[200];
        std::snprintf(summary, sizeof(summary), "Benchmark: median %.2f ms, p95 %.2f ms - out/%s",
                      m_benchmarkFrames.Median(), m_benchmarkFrames.Percentile(0.95), file.filename().string().c_str());
        m_benchmarkResult = summary;
        ++m_benchmarksDone;
        // The village as it was: the clock's place, the player's spot.
        ResetEnvironment();
        ApplyLighting();
        m_clockOn = !m_diagnostics.HasTestScript();
        m_player.Teleport(m_player.GetFeetPosition(), m_camera);
        OpenMenu();
    }

    void ShowcaseApp::DrawScreens()
    {
        UiKit kit(GetRenderer().GetUI(), *m_uiFonts);
        switch (m_screen)
        {
        case Screen::Menu:
            m_menu.Draw(kit, 0.35f);
            break;
        case Screen::Settings:
            m_settingsScreen.Draw(kit, m_store.Settings(), m_resolved.gpu);
            break;
        case Screen::Benchmark:
        {
            // A thin progress bar and the time of day; nothing else on screen.
            const glm::vec2 screen = kit.Screen();
            const float width = screen.x * 0.3f;
            const glm::vec2 at{ (screen.x - width) * 0.5f, screen.y - kit.Margin() * 2.0f };
            kit.Renderer().DrawRect(at, { width, 3.0f * kit.Scale() }, kit.Theme().panel);
            kit.Renderer().DrawRect(at, { width * std::clamp(m_benchmarkTime / m_benchmarkSeconds, 0.0f, 1.0f), 3.0f * kit.Scale() },
                                    kit.Theme().accent);
            kit.Text("Benchmark - " + m_environmentName + "    Esc stop", at - glm::vec2{ 0.0f, kit.LineHeight() * 1.2f },
                     kit.Theme().dim);
            break;
        }
        case Screen::None:
            break;
        }
    }
}
