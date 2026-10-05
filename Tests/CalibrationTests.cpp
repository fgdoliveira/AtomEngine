#include "Settings/Calibration.h"

#include <doctest/doctest.h>

using namespace AtomGame;
using Outcome = CalibrationDecision::Outcome;

namespace
{
    // Two views (the city's lights, the lake's water and weather) at three
    // tiers: each argument is that tier's p95 in the city; the lake adds
    // `lakeExtra`; medians are 60 % of the p95.
    CalibrationInput Measured(double high, double balanced, double low, double lakeExtra = 0.0)
    {
        CalibrationInput input;
        input.refreshHz = 144.0;
        for (auto [tier, p95] : { std::pair{ QualityTier::High, high }, std::pair{ QualityTier::Balanced, balanced },
                                  std::pair{ QualityTier::Low, low } })
        {
            input.samples.push_back({ tier, "night_street", p95 * 0.6, p95 });
            input.samples.push_back({ tier, "lakeshore", (p95 + lakeExtra) * 0.6, p95 + lakeExtra });
        }
        return input;
    }
}

TEST_CASE("The highest tier within the 13.3 ms budget wins")
{
    CHECK(DecideCalibration(Measured(9.0, 6.0, 4.0)).tier == QualityTier::High);
    const CalibrationDecision balanced = DecideCalibration(Measured(15.0, 11.0, 7.0));
    CHECK(balanced.outcome == Outcome::Selected);
    CHECK(balanced.tier == QualityTier::Balanced);
    CHECK(balanced.worstP95Ms == doctest::Approx(11.0));
    CHECK(DecideCalibration(Measured(20.0, 16.0, 12.0)).tier == QualityTier::Low);

    // Exactly the budget passes.
    CHECK(DecideCalibration(Measured(13.3, 9.0, 6.0)).tier == QualityTier::High);
}

TEST_CASE("The worst scene decides: one heavy view keeps a tier out")
{
    // High is fine in the city (12 ms) but the lake adds 3 ms: 15 > 13.3.
    const CalibrationDecision decision = DecideCalibration(Measured(12.0, 8.0, 5.0, 3.0));
    CHECK(decision.tier == QualityTier::Balanced);
    CHECK(decision.worstP95Ms == doctest::Approx(11.0)); // 8 + 3
}

TEST_CASE("Nothing passes: Low, with the reason")
{
    const CalibrationDecision decision = DecideCalibration(Measured(30.0, 24.0, 18.0));
    CHECK(decision.outcome == Outcome::NonePassed);
    CHECK(decision.tier == QualityTier::Low);
    CHECK(decision.message.find("below the tested range") != std::string::npos);
}

TEST_CASE("Battery and vsync make a calibration impossible, not wrong")
{
    CalibrationInput battery = Measured(9.0, 6.0, 4.0);
    battery.onBattery = true;
    CHECK(DecideCalibration(battery).outcome == Outcome::Refused);

    CalibrationInput vsync = Measured(9.0, 6.0, 4.0);
    vsync.vsyncPresent = true;
    CHECK(DecideCalibration(vsync).outcome == Outcome::Inconclusive);

    CHECK(DecideCalibration(CalibrationInput{}).outcome == Outcome::Inconclusive);
}

TEST_CASE("Frame times sitting on the refresh period are inconclusive")
{
    // A 60 Hz display holding every view near 16.7 ms, tightly clustered.
    CHECK(LooksRefreshCapped(16.7, 17.2, 60.0));
    CHECK(LooksRefreshCapped(33.3, 34.0, 60.0));  // every other refresh
    CHECK_FALSE(LooksRefreshCapped(9.0, 12.0, 60.0));
    CHECK_FALSE(LooksRefreshCapped(16.7, 24.0, 60.0)); // spread: the GPU, not the display
    CHECK_FALSE(LooksRefreshCapped(16.7, 17.0, 0.0));  // unknown refresh: can't tell

    CalibrationInput capped;
    capped.refreshHz = 60.0;
    for (QualityTier tier : { QualityTier::High, QualityTier::Balanced, QualityTier::Low })
    {
        capped.samples.push_back({ tier, "night_street", 16.7, 17.1 });
        capped.samples.push_back({ tier, "lakeshore", 16.6, 17.0 });
    }
    const CalibrationDecision decision = DecideCalibration(capped);
    CHECK(decision.outcome == Outcome::Inconclusive);
    CHECK(decision.message.find("laptop's own screen") != std::string::npos);
}
