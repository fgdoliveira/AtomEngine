#pragma once

#include "Settings/GameSettings.h"

#include <string>
#include <vector>

namespace AtomGame
{
    // Auto quality (M64): the highest tier this machine draws within budget,
    // measured, not guessed from a GPU's name. Pure decisions here; the run
    // that measures lives in DemoAppCalibration.cpp.

    // A stable 60 fps leaves 16.7 ms a frame; 13.3 ms keeps 20 % of it in
    // reserve for what a fixed camera doesn't show (turning, effects, heat).
    inline constexpr double CalibrationBudgetMs = 13.3;

    // One measured view at one tier.
    struct CalibrationSample
    {
        QualityTier tier = QualityTier::High;
        std::string scene;
        double medianMs = 0.0;
        double p95Ms = 0.0;
    };

    struct CalibrationInput
    {
        std::vector<CalibrationSample> samples;
        bool onBattery = false;    // refused: battery made this laptop ~3x slower
        double refreshHz = 0.0;    // the window's display; 0 if unknown
        bool vsyncPresent = false; // presentation waits for the display: can't measure
    };

    struct CalibrationDecision
    {
        enum class Outcome
        {
            Selected,     // a tier passed: Auto uses it
            NonePassed,   // even Low misses the budget: Low, with a warning
            Inconclusive, // frame times held to the display: nothing learnt
            Refused,      // on battery: not run
        };
        Outcome outcome = Outcome::Inconclusive;
        QualityTier tier = QualityTier::High;
        double worstMedianMs = 0.0; // of the chosen tier, over every scene
        double worstP95Ms = 0.0;
        std::string message;
    };

    // Do a sample's frame times sit on the display's refresh period - the
    // presentation waiting, not the GPU? (A tight cluster at 16.7 ms on a
    // 60 Hz screen, say.)
    bool LooksRefreshCapped(double medianMs, double p95Ms, double refreshHz);

    // The highest tier whose worst p95 over every scene is within budget.
    CalibrationDecision DecideCalibration(const CalibrationInput& input);
}
