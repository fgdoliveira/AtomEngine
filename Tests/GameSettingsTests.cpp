#include "Settings/GameSettings.h"

#include <doctest/doctest.h>

#include <string>
#include <vector>

using namespace AtomFramework;

namespace
{
    CommandLine Args(std::vector<std::string> args)
    {
        return ParseCommandLine(args);
    }
}

TEST_CASE("Defaults are today's look on the stable adapter")
{
    const GameSettings defaults;
    CHECK(defaults.gpu == GpuPreference::LowPower);
    CHECK(defaults.quality == QualityMode::High);
    const QualityPreset high = PresetFor(QualityTier::High);
    CHECK(high.renderScale == doctest::Approx(1.0f));
    CHECK(high.msaaSamples == 4);
    CHECK(high.shadows);
    CHECK(high.particles);
    CHECK(high.reflection);
}

TEST_CASE("Presets: Low and Balanced give things up; changing a switch makes it Custom")
{
    const QualityPreset low = PresetFor(QualityTier::Low);
    CHECK(low.renderScale == doctest::Approx(0.5f));
    CHECK(low.msaaSamples == 1);
    CHECK_FALSE(low.shadows);
    CHECK_FALSE(low.particles);
    CHECK_FALSE(low.reflection);
    const QualityPreset balanced = PresetFor(QualityTier::Balanced);
    CHECK(balanced.renderScale == doctest::Approx(0.75f));
    CHECK(balanced.msaaSamples == 2);
    CHECK(balanced.shadows);

    for (QualityTier tier : { QualityTier::Low, QualityTier::Balanced, QualityTier::High })
    {
        CHECK(TierOf(PresetFor(tier)) == tier);
    }
    QualityPreset edited = PresetFor(QualityTier::High);
    edited.shadows = false; // F6
    CHECK(TierOf(edited) == QualityTier::Custom);
    CHECK(TierOf(PresetFor(QualityTier::High)) == QualityTier::High); // the preset itself is unchanged
}

TEST_CASE("Settings round-trip through their file")
{
    GameSettings settings;
    settings.gpu = GpuPreference::HighPerformance;
    settings.quality = QualityMode::Auto;
    settings.pendingFallback = GpuPreference::LowPower;
    settings.calibrateNextLaunch = true;
    settings.calibration = CalibrationRecord{ CalibrationRecord::CurrentVersion, "Intel(R) Iris(R) Xe Graphics", "direct3d12",
                                              1280, 720, 4, QualityTier::Balanced, 7.5, 12.1 };
    const SettingsLoad load = ParseSettings(WriteSettings(settings));
    CHECK(load.warning.empty());
    CHECK(load.settings.gpu == GpuPreference::HighPerformance);
    CHECK(load.settings.quality == QualityMode::Auto);
    REQUIRE(load.settings.pendingFallback.has_value());
    CHECK(*load.settings.pendingFallback == GpuPreference::LowPower);
    CHECK(load.settings.calibrateNextLaunch);
    REQUIRE(load.settings.calibration.has_value());
    CHECK(load.settings.calibration->adapter == "Intel(R) Iris(R) Xe Graphics");
    CHECK(load.settings.calibration->tier == QualityTier::Balanced);
    CHECK(load.settings.calibration->p95Ms == doctest::Approx(12.1));
}

TEST_CASE("The player's display and volume (M90) round-trip; an older file reads as the defaults")
{
    GameSettings settings;
    settings.fullscreen = true;
    settings.volume = 0.3f;
    const SettingsLoad load = ParseSettings(WriteSettings(settings));
    CHECK(load.warning.empty());
    CHECK(load.settings.fullscreen);
    CHECK(load.settings.volume == doctest::Approx(0.3f));

    const SettingsLoad older = ParseSettings(R"({ "schema": 1, "gpu": "low-power", "quality": "high" })");
    CHECK(older.warning.empty());
    CHECK_FALSE(older.settings.fullscreen);
    CHECK(older.settings.volume == doctest::Approx(0.8f));
    CHECK(ParseSettings(R"({ "schema": 1, "volume": 7 })").settings.volume == doctest::Approx(1.0f)); // clamped
}

TEST_CASE("A bad settings file never stops the game: defaults and a warning")
{
    const SettingsLoad none = ParseSettings("");
    CHECK(none.warning.empty()); // first run: no file, nothing to say
    CHECK(none.settings.quality == QualityMode::High);

    const SettingsLoad broken = ParseSettings("{ not json");
    CHECK_FALSE(broken.warning.empty());
    CHECK(broken.settings.gpu == GpuPreference::LowPower);

    const SettingsLoad future = ParseSettings(R"({ "schema": 99, "gpu": "high-performance" })");
    CHECK(future.warning.find("schema 99") != std::string::npos);
    CHECK(future.settings.gpu == GpuPreference::LowPower); // a newer file isn't guessed at

    const SettingsLoad unversioned = ParseSettings(R"({ "gpu": "high-performance" })");
    CHECK_FALSE(unversioned.warning.empty());

    const SettingsLoad odd = ParseSettings(R"({ "schema": 1, "gpu": "turbo", "quality": "low" })");
    CHECK(odd.warning.find("gpu") != std::string::npos);
    CHECK(odd.settings.gpu == GpuPreference::LowPower); // the bad value alone is dropped
    CHECK(odd.settings.quality == QualityMode::Low);    // the good one kept
}

TEST_CASE("The command line: known flags, values, and errors that aren't fatal")
{
    const CommandLine line = Args({ "--gpu", "high-performance", "--quality", "balanced", "--calibrate",
                                    "--diagnostics", "out.txt", "--no-settings", "--reset-settings" });
    CHECK(line.errors.empty());
    CHECK(*line.gpu == GpuPreference::HighPerformance);
    CHECK(*line.quality == QualityMode::Balanced);
    CHECK(line.calibrate);
    CHECK(*line.diagnosticsFile == "out.txt");
    CHECK(line.noSettings);
    CHECK(line.resetSettings);

    CHECK(Args({}).errors.empty());
    const CommandLine bad = Args({ "--gpu", "turbo", "--quality", "--fast", "--diagnostics" });
    CHECK(bad.errors.size() == 4); // bad value, missing value, unknown flag, missing value
    CHECK_FALSE(bad.gpu.has_value());
    CHECK_FALSE(bad.quality.has_value());
}

TEST_CASE("Precedence: command line, then environment, then saved, then defaults")
{
    GameSettings saved;
    saved.gpu = GpuPreference::HighPerformance;
    saved.quality = QualityMode::Low;

    const ResolvedSettings fromSaved = ResolveSettings({}, {}, &saved);
    CHECK(fromSaved.gpu == GpuPreference::HighPerformance);
    CHECK(fromSaved.quality == QualityMode::Low);

    const ResolvedSettings fromEnvironment = ResolveSettings({}, { GpuPreference::LowPower, QualityMode::Balanced }, &saved);
    CHECK(fromEnvironment.gpu == GpuPreference::LowPower);
    CHECK(fromEnvironment.quality == QualityMode::Balanced);
    CHECK(fromEnvironment.gpuReason == "ATOM_GPU");

    CommandLine line;
    line.quality = QualityMode::High;
    const ResolvedSettings fromLine = ResolveSettings(line, { std::nullopt, QualityMode::Balanced }, &saved);
    CHECK(fromLine.quality == QualityMode::High);
    CHECK(fromLine.gpu == GpuPreference::HighPerformance); // nothing overrode the saved GPU

    // Tests, benchmarks, --no-settings, --reset-settings: saved settings
    // don't count, so every machine runs them the same way.
    const ResolvedSettings ignored = ResolveSettings({}, {}, nullptr);
    CHECK(ignored.gpu == GpuPreference::LowPower);
    CHECK(ignored.quality == QualityMode::High);
}

TEST_CASE("A pending fallback beats the saved GPU, but not an explicit choice")
{
    GameSettings saved;
    saved.gpu = GpuPreference::HighPerformance;
    saved.pendingFallback = GpuPreference::LowPower;
    const ResolvedSettings next = ResolveSettings({}, {}, &saved);
    CHECK(next.gpu == GpuPreference::LowPower);
    CHECK(next.gpuReason.find("fallback") != std::string::npos);

    CommandLine line;
    line.gpu = GpuPreference::HighPerformance;
    CHECK(ResolveSettings(line, {}, &saved).gpu == GpuPreference::HighPerformance);
}

TEST_CASE("Only the high-performance adapter falls back after a failure (M63)")
{
    CHECK(FallbackAfterFailure(GpuPreference::HighPerformance) == GpuPreference::LowPower);
    CHECK_FALSE(FallbackAfterFailure(GpuPreference::LowPower).has_value());

    // Recorded, the next launch uses it and says why; an explicit --gpu wins.
    GameSettings saved;
    saved.gpu = GpuPreference::HighPerformance;
    saved.pendingFallback = FallbackAfterFailure(saved.gpu);
    const SettingsLoad reloaded = ParseSettings(WriteSettings(saved));
    const ResolvedSettings next = ResolveSettings({}, {}, &reloaded.settings);
    CHECK(next.gpu == GpuPreference::LowPower);
    CHECK(next.gpuReason.find("fallback") != std::string::npos);
}

TEST_CASE("Auto uses a calibration only for the same version and adapter")
{
    CalibrationRecord record;
    record.adapter = "Intel(R) Iris(R) Xe Graphics";
    record.tier = QualityTier::Balanced;

    CHECK(TierFor(QualityMode::Auto, record, "Intel(R) Iris(R) Xe Graphics") == QualityTier::Balanced);
    CHECK(TierFor(QualityMode::Auto, record, "NVIDIA GeForce RTX 4060 Laptop GPU") == QualityTier::High);
    CHECK(TierFor(QualityMode::Auto, std::nullopt, "Intel(R) Iris(R) Xe Graphics") == QualityTier::High);
    record.version = CalibrationRecord::CurrentVersion + 1;
    CHECK(TierFor(QualityMode::Auto, record, "Intel(R) Iris(R) Xe Graphics") == QualityTier::High);

    CHECK(TierFor(QualityMode::Low, std::nullopt, "") == QualityTier::Low);
    CHECK(TierFor(QualityMode::Balanced, std::nullopt, "") == QualityTier::Balanced);
    CHECK(TierFor(QualityMode::High, record, "") == QualityTier::High);
}
