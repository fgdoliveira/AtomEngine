#include "Settings/Calibration.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace AtomFramework
{
    bool LooksRefreshCapped(double medianMs, double p95Ms, double refreshHz)
    {
        if (refreshHz <= 0.0 || medianMs <= 0.0)
        {
            return false;
        }
        const double period = 1000.0 / refreshHz;
        // On (a multiple of) the period, and tightly clustered there.
        const double multiple = std::max(1.0, std::round(medianMs / period));
        const double target = multiple * period;
        return std::abs(medianMs - target) < 0.07 * period && (p95Ms - medianMs) < 0.15 * period;
    }

    CalibrationDecision DecideCalibration(const CalibrationInput& input)
    {
        CalibrationDecision decision;
        if (input.onBattery)
        {
            decision.outcome = CalibrationDecision::Outcome::Refused;
            decision.message = "on battery: plug in and calibrate again (battery power slows this kind of laptop ~3x)";
            return decision;
        }
        if (input.vsyncPresent || input.samples.empty())
        {
            decision.message = input.samples.empty() ? "nothing was measured"
                                                     : "presentation waits for the display (vsync): frame times say nothing";
            return decision; // Inconclusive
        }

        for (QualityTier tier : { QualityTier::High, QualityTier::Balanced, QualityTier::Low })
        {
            double worstMedian = 0.0;
            double worstP95 = 0.0;
            bool any = false;
            bool allCapped = true;
            for (const CalibrationSample& sample : input.samples)
            {
                if (sample.tier != tier)
                {
                    continue;
                }
                any = true;
                worstMedian = std::max(worstMedian, sample.medianMs);
                worstP95 = std::max(worstP95, sample.p95Ms);
                allCapped = allCapped && LooksRefreshCapped(sample.medianMs, sample.p95Ms, input.refreshHz);
            }
            if (!any)
            {
                continue;
            }
            if (allCapped)
            {
                // Every view of this tier sits on the refresh period: the
                // display set the pace, so how fast the machine is is unknown.
                decision.message = "frame times are held to the display's refresh: move the window to the laptop's "
                                   "own screen and calibrate again";
                return decision; // Inconclusive
            }
            if (worstP95 <= CalibrationBudgetMs)
            {
                decision.outcome = CalibrationDecision::Outcome::Selected;
                decision.tier = tier;
                decision.worstMedianMs = worstMedian;
                decision.worstP95Ms = worstP95;
                char text[160];
                std::snprintf(text, sizeof(text), "%s: worst p95 %.1f ms, within the %.1f ms budget",
                              std::string(ToString(tier)).c_str(), worstP95, CalibrationBudgetMs);
                decision.message = text;
                return decision;
            }
            decision.worstMedianMs = worstMedian;
            decision.worstP95Ms = worstP95;
        }

        decision.outcome = CalibrationDecision::Outcome::NonePassed;
        decision.tier = QualityTier::Low;
        char text[200];
        std::snprintf(text, sizeof(text),
                      "even Low misses the budget (worst p95 %.1f ms > %.1f ms): this machine is below the tested range; "
                      "Low is the best it can do",
                      decision.worstP95Ms, CalibrationBudgetMs);
        decision.message = text;
        return decision;
    }
}
