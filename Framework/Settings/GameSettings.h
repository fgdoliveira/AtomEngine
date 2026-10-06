#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace AtomFramework // M76: shared by every game
{
    // The player's machine-level choices (M59, v0.0.10 hardware
    // portability), kept apart on purpose:
    // - which GPU to prefer: a *stability* choice. Low-power is the default
    //   because a hybrid laptop's high-performance adapter lost its
    //   swapchain when the window crossed to the other adapter's display;
    // - how much to draw: a *quality* choice.
    // Pure data and functions: parsed, resolved and tested without SDL.

    enum class GpuPreference { LowPower, HighPerformance };

    // What the player asked for. Auto uses the last calibration (M64).
    enum class QualityMode { Auto, Low, Balanced, High };

    // What is applied. Custom: individual switches (F-keys, F10) moved away
    // from every preset; the presets themselves never change.
    enum class QualityTier { Low, Balanced, High, Custom };

    // A tier's settings. High is the look every version so far rendered.
    struct QualityPreset
    {
        float renderScale = 1.0f;
        std::uint32_t msaaSamples = 4;
        bool shadows = true;
        bool particles = true;
        bool reflection = true; // allows the levels that ask for one (M51)

        bool operator==(const QualityPreset&) const = default;
    };

    QualityPreset PresetFor(QualityTier tier); // Custom: High's values
    // The tier these settings are exactly, or Custom.
    QualityTier TierOf(const QualityPreset& settings);

    // What Auto picked, and on what (M64): valid only for this calibration
    // version on the same adapter.
    struct CalibrationRecord
    {
        static constexpr int CurrentVersion = 1;
        int version = CurrentVersion;
        std::string adapter;
        std::string backend;
        std::uint32_t width = 0;
        std::uint32_t height = 0;
        std::uint32_t msaaSamples = 0;
        QualityTier tier = QualityTier::High;
        double medianMs = 0.0;
        double p95Ms = 0.0;
    };
    bool IsCalibrationValid(const CalibrationRecord& record, std::string_view adapter);

    // What is saved between runs (per user, under SDL's pref path - M61).
    struct GameSettings
    {
        static constexpr int SchemaVersion = 1;
        GpuPreference gpu = GpuPreference::LowPower;
        QualityMode quality = QualityMode::High;
        std::optional<CalibrationRecord> calibration;
        // Set when the high-performance adapter lost its swapchain (M63):
        // the next launch uses this preference instead, and says why.
        std::optional<GpuPreference> pendingFallback;
        bool calibrateNextLaunch = false; // asked for in F10 (M61); run by M64
    };

    // Never fails: a missing, malformed or unknown-version file gives the
    // defaults and a warning to show, so a bad file can't stop the game.
    struct SettingsLoad
    {
        GameSettings settings;
        std::string warning; // "" when the file was read as is
    };
    SettingsLoad ParseSettings(std::string_view json);
    std::string WriteSettings(const GameSettings& settings);

    // The command line (wired in M60): --gpu low-power|high-performance,
    // --quality auto|low|balanced|high, --calibrate, --diagnostics <file>,
    // --reset-settings, --no-settings. Unknown or malformed arguments are
    // reported in `errors`, never fatal.
    struct CommandLine
    {
        std::optional<GpuPreference> gpu;
        std::optional<QualityMode> quality;
        bool calibrate = false;
        bool resetSettings = false; // forget the saved settings
        bool noSettings = false;    // ignore them for this run (tests, benches)
        std::optional<std::string> diagnosticsFile;
        std::vector<std::string> errors;
    };
    CommandLine ParseCommandLine(std::span<const std::string> args);

    // The ATOM_* environment's say (ATOM_GPU, ATOM_QUALITY).
    struct EnvironmentOverrides
    {
        std::optional<GpuPreference> gpu;
        std::optional<QualityMode> quality;
    };

    // The effective choices: command line > environment > saved > defaults.
    // `saved` is null when saved settings must not count - a scripted test
    // or benchmark run, --no-settings or --reset-settings - so those runs
    // behave the same on every machine whatever was last chosen.
    struct ResolvedSettings
    {
        GpuPreference gpu = GpuPreference::LowPower;
        QualityMode quality = QualityMode::High;
        std::string gpuReason; // why this preference (shown in diagnostics)
    };
    ResolvedSettings ResolveSettings(const CommandLine& commandLine,
                                     const EnvironmentOverrides& environment,
                                     const GameSettings* saved);

    // The tier to apply for a mode: Auto uses a valid calibration for this
    // adapter, else High (today's look) until calibrated.
    QualityTier TierFor(QualityMode mode, const std::optional<CalibrationRecord>& calibration,
                        std::string_view adapter);

    // After the device failed mid-run (M63): the preference to use next
    // launch, if a safer one exists. Only high-performance has a fallback -
    // low-power is already the known-good choice.
    std::optional<GpuPreference> FallbackAfterFailure(GpuPreference active);

    // Names as written in files and on the command line.
    std::string_view ToString(GpuPreference value);
    std::string_view ToString(QualityMode value);
    std::string_view ToString(QualityTier value);
    std::optional<GpuPreference> ParseGpuPreference(std::string_view text);
    std::optional<QualityMode> ParseQualityMode(std::string_view text);
    std::optional<QualityTier> ParseQualityTier(std::string_view text);
}
