// Auto calibration (M64): DemoApp's measured run. The decision itself is
// pure and tested (Settings/Calibration.*); this file drives the game
// through the views and records what it saw.
#include "DemoApp.h"
#include "Core/DevSwitch.h" // M82: ATOM_* switches, compiled out of packages

#include <SDL3/SDL.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <iostream>

namespace Demo
{
    namespace
    {
        // The heaviest views the game has: the night street's lights and
        // particles, and the windmill field in rain (foliage, shadows,
        // drops). v0.0.14: was the lakeshore, a lab players never reached,
        // which moved to the Showcase.
        struct CalibrationView
        {
            const char* level;
            bool place;
            glm::vec3 feet;
            float yawDegrees;
            float rain;
        };
        constexpr std::array<CalibrationView, 2> Views{ {
            { "night_street", false, {}, 0.0f, 0.0f },
            { "windmill_field", false, {}, 0.0f, 1.0f },
        } };
        // Each view measures every tier twice, in the order H B L L B H (the
        // ABBA of M46: drift lands on both halves), and keeps each tier's
        // better pass - a background stall only ever slows a pass, and one
        // 3 s window's p95 is easily moved by it (seen on this laptop).
        constexpr std::array<QualityTier, 6> Tiers{ QualityTier::High, QualityTier::Balanced, QualityTier::Low,
                                                    QualityTier::Low, QualityTier::Balanced, QualityTier::High };

        bool OnBattery()
        {
            int seconds = -1;
            int percent = -1;
            return SDL_GetPowerInfo(&seconds, &percent) == SDL_POWERSTATE_ON_BATTERY;
        }
    }

    void DemoApp::StartCalibration(bool quitAfter)
    {
        if (m_calibration.active)
        {
            return;
        }
        if (OnBattery())
        {
            // Refused before anything runs: on battery this laptop measured
            // ~3x slower (M46) - Auto would wrongly pick Low.
            std::cout << "Calibration: refused - on battery. Plug in and calibrate again." << std::endl;
            m_messages.Show("Calibration needs mains power: plug in and try again.");
            m_savedSettings.calibrateNextLaunch = false;
            SaveSettings();
            if (quitAfter)
            {
                RequestQuit(2);
            }
            return;
        }

        m_calibration = CalibrationRun{};
        m_calibration.active = true;
        m_calibration.quitAfter = quitAfter;
        m_calibration.returnLevel = LevelName();
        // ATOM_CALIBRATE_SECONDS shortens the run (tests); settling stays
        // about a third of it.
        if (const char* seconds = Atom::DevSwitch("ATOM_CALIBRATE_SECONDS"); seconds && *seconds)
        {
            m_calibration.measureSeconds = std::max(0.25f, static_cast<float>(SDL_atof(seconds)));
            m_calibration.settleSeconds = std::min(1.0f, m_calibration.measureSeconds / 3.0f);
        }
        GetRenderer().SetUncappedPresentation(true);
        // Measured at the frames in flight the player runs: calibrating with
        // a deeper queue than play would overrate the machine.
        std::cout << "Calibration: started (" << Views.size() << " views x 3 tiers, each twice, "
                  << GetRenderer().GetDeviceReport().framesInFlight << " frames in flight"
                  << "; keep the window on this screen and don't touch it)" << std::endl;
        m_messages.Show("Calibrating... hands off for about a minute.");
    }

    void DemoApp::UpdateCalibration(float realSeconds)
    {
        CalibrationRun& run = m_calibration;
        const CalibrationView& view = Views[run.scene];
        switch (run.phase)
        {
        case CalibrationRun::Phase::Load:
            if (LevelName() != view.level)
            {
                if (!m_levels->IsTransitioning())
                {
                    m_levels->RequestChange(view.level, "");
                }
                return;
            }
            if (m_levels->IsTransitioning() || m_mode != Mode::Exploring)
            {
                return;
            }
            if (view.place)
            {
                Teleport(view.feet, view.yawDegrees);
            }
            m_view.rain = view.rain > 0.0f ? std::optional<float>(view.rain) : std::nullopt;
            ApplyQuality(Tiers[run.tier]);
            run.phase = CalibrationRun::Phase::Settle;
            run.timer = 0.0f;
            return;

        case CalibrationRun::Phase::Settle:
            // A tier change rebuilds targets and pipelines: not steady state.
            run.timer += realSeconds;
            if (run.timer >= run.settleSeconds)
            {
                run.phase = CalibrationRun::Phase::Measure;
                run.timer = 0.0f;
                run.window.Clear();
            }
            return;

        case CalibrationRun::Phase::Measure:
            run.window.AddSample(static_cast<double>(realSeconds) * 1000.0);
            run.timer += realSeconds;
            if (run.timer < run.measureSeconds)
            {
                return;
            }
        {
            const double median = run.window.Median();
            const double p95 = run.window.Percentile(0.95);
            const QualityTier tier = Tiers[run.tier];
            // The second pass of a (view, tier) keeps the better of the two.
            bool merged = false;
            for (CalibrationSample& sample : run.samples)
            {
                if (sample.tier == tier && sample.scene == view.level)
                {
                    sample.medianMs = std::min(sample.medianMs, median);
                    sample.p95Ms = std::min(sample.p95Ms, p95);
                    merged = true;
                }
            }
            if (!merged)
            {
                run.samples.push_back({ tier, view.level, median, p95 });
            }
            std::printf("Calibration: %-12s %-8s median %6.2f ms  p95 %6.2f ms%s\n", view.level,
                        std::string(ToString(tier)).c_str(), median, p95, merged ? "  (2nd pass)" : "");
            std::fflush(stdout);
        }
            if (++run.tier < Tiers.size())
            {
                ApplyQuality(Tiers[run.tier]);
                run.phase = CalibrationRun::Phase::Settle;
                run.timer = 0.0f;
                return;
            }
            run.tier = 0;
            if (++run.scene < Views.size())
            {
                run.phase = CalibrationRun::Phase::Load;
                return;
            }
            FinishCalibration();
            return;
        }
    }

    void DemoApp::FinishCalibration()
    {
        CalibrationRun& run = m_calibration;
        run.active = false;
        m_view.rain.reset();

        Atom::Renderer& renderer = GetRenderer();
        CalibrationInput input;
        input.samples = run.samples;
        input.onBattery = OnBattery();
        input.vsyncPresent = renderer.GetDeviceReport().presentMode == "vsync";
        if (const SDL_DisplayMode* mode = SDL_GetCurrentDisplayMode(SDL_GetDisplayForWindow(GetWindow().GetSDLWindow())))
        {
            input.refreshHz = mode->refresh_rate;
        }
        const CalibrationDecision decision = DecideCalibration(input);
        renderer.SetUncappedPresentation(false); // back to how this run presents

        const bool measured = decision.outcome == CalibrationDecision::Outcome::Selected
            || decision.outcome == CalibrationDecision::Outcome::NonePassed;
        std::cout << "Calibration: " << (measured ? ToString(decision.tier) : std::string_view("no result"))
                  << " - " << decision.message << std::endl;
        m_messages.Show("Calibration: " + decision.message);

        if (measured)
        {
            CalibrationRecord record;
            record.adapter = renderer.GetAdapterName();
            record.backend = renderer.GetBackendName();
            int width = 0;
            int height = 0;
            SDL_GetWindowSizeInPixels(GetWindow().GetSDLWindow(), &width, &height);
            record.width = static_cast<std::uint32_t>(width);
            record.height = static_cast<std::uint32_t>(height);
            record.msaaSamples = PresetFor(decision.tier).msaaSamples;
            record.tier = decision.tier;
            record.medianMs = decision.worstMedianMs;
            record.p95Ms = decision.worstP95Ms;
            m_savedSettings.calibration = record;
            m_savedSettings.quality = QualityMode::Auto; // use it
        }
        m_savedSettings.calibrateNextLaunch = false;
        SaveSettings(); // only when this run uses saved settings
        SetQualityMode(measured ? QualityMode::Auto : m_resolvedSettings.quality, false);

        if (run.quitAfter)
        {
            RequestQuit(measured ? 0 : 2);
        }
        else if (!run.returnLevel.empty() && run.returnLevel != LevelName())
        {
            m_levels->RequestChange(run.returnLevel, "");
        }
    }
}
