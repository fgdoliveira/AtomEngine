#include "Settings/GameSettings.h"

#include <nlohmann/json.hpp>

namespace AtomGame
{
    using Json = nlohmann::json;

    QualityPreset PresetFor(QualityTier tier)
    {
        switch (tier)
        {
        case QualityTier::Low:
            return { 0.5f, 1, false, false, false };
        case QualityTier::Balanced:
            return { 0.75f, 2, true, true, false };
        case QualityTier::High:
        case QualityTier::Custom:
            break;
        }
        return {}; // High: native scale, 4x MSAA, everything on
    }

    QualityTier TierOf(const QualityPreset& settings)
    {
        for (QualityTier tier : { QualityTier::Low, QualityTier::Balanced, QualityTier::High })
        {
            if (PresetFor(tier) == settings)
            {
                return tier;
            }
        }
        return QualityTier::Custom;
    }

    bool IsCalibrationValid(const CalibrationRecord& record, std::string_view adapter)
    {
        return record.version == CalibrationRecord::CurrentVersion && !record.adapter.empty()
            && record.adapter == adapter && record.tier != QualityTier::Custom;
    }

    std::string_view ToString(GpuPreference value)
    {
        return value == GpuPreference::HighPerformance ? "high-performance" : "low-power";
    }

    std::string_view ToString(QualityMode value)
    {
        switch (value)
        {
        case QualityMode::Auto: return "auto";
        case QualityMode::Low: return "low";
        case QualityMode::Balanced: return "balanced";
        case QualityMode::High: break;
        }
        return "high";
    }

    std::string_view ToString(QualityTier value)
    {
        switch (value)
        {
        case QualityTier::Low: return "low";
        case QualityTier::Balanced: return "balanced";
        case QualityTier::Custom: return "custom";
        case QualityTier::High: break;
        }
        return "high";
    }

    std::optional<GpuPreference> ParseGpuPreference(std::string_view text)
    {
        if (text == "low-power") return GpuPreference::LowPower;
        if (text == "high-performance") return GpuPreference::HighPerformance;
        return std::nullopt;
    }

    std::optional<QualityMode> ParseQualityMode(std::string_view text)
    {
        if (text == "auto") return QualityMode::Auto;
        if (text == "low") return QualityMode::Low;
        if (text == "balanced") return QualityMode::Balanced;
        if (text == "high") return QualityMode::High;
        return std::nullopt;
    }

    std::optional<QualityTier> ParseQualityTier(std::string_view text)
    {
        if (text == "low") return QualityTier::Low;
        if (text == "balanced") return QualityTier::Balanced;
        if (text == "high") return QualityTier::High;
        return std::nullopt; // "custom" is never saved as a calibration result
    }

    namespace
    {
        template <typename T, typename Parse>
        std::optional<T> ReadEnum(const Json& object, const char* key, Parse parse, std::string& warning)
        {
            const auto found = object.find(key);
            if (found == object.end())
            {
                return std::nullopt;
            }
            if (found->is_string())
            {
                if (auto value = parse(found->template get<std::string>()))
                {
                    return value;
                }
            }
            warning = std::string("unknown value for \"") + key + "\"";
            return std::nullopt;
        }
    }

    SettingsLoad ParseSettings(std::string_view text)
    {
        SettingsLoad result;
        if (text.empty())
        {
            return result; // no file yet: defaults, nothing to say
        }
        const Json root = Json::parse(text, nullptr, false);
        if (root.is_discarded() || !root.is_object())
        {
            result.warning = "settings file is not valid JSON; using defaults";
            return result;
        }
        const int schema = root.value("schema", 0);
        if (schema != GameSettings::SchemaVersion)
        {
            // Only one version exists yet; a newer file (written by a later
            // build) or an unversioned one isn't guessed at.
            result.warning = "settings file has schema " + std::to_string(schema) + ", expected "
                + std::to_string(GameSettings::SchemaVersion) + "; using defaults";
            return result;
        }

        GameSettings& s = result.settings;
        std::string problem;
        if (auto gpu = ReadEnum<GpuPreference>(root, "gpu", ParseGpuPreference, problem)) s.gpu = *gpu;
        if (auto quality = ReadEnum<QualityMode>(root, "quality", ParseQualityMode, problem)) s.quality = *quality;
        s.pendingFallback = ReadEnum<GpuPreference>(root, "pendingFallback", ParseGpuPreference, problem);
        if (const auto calibrate = root.find("calibrateNextLaunch"); calibrate != root.end() && calibrate->is_boolean())
        {
            s.calibrateNextLaunch = calibrate->get<bool>();
        }

        if (const auto c = root.find("calibration"); c != root.end() && c->is_object())
        {
            CalibrationRecord record;
            record.version = c->value("version", 0);
            record.adapter = c->value("adapter", std::string{});
            record.backend = c->value("backend", std::string{});
            record.width = c->value("width", 0u);
            record.height = c->value("height", 0u);
            record.msaaSamples = c->value("msaa", 0u);
            record.medianMs = c->value("medianMs", 0.0);
            record.p95Ms = c->value("p95Ms", 0.0);
            if (auto tier = ReadEnum<QualityTier>(*c, "tier", ParseQualityTier, problem))
            {
                record.tier = *tier;
                s.calibration = record;
            }
        }
        if (!problem.empty())
        {
            result.warning = "settings file: " + problem + " (ignored)";
        }
        return result;
    }

    std::string WriteSettings(const GameSettings& s)
    {
        Json root;
        root["schema"] = GameSettings::SchemaVersion;
        root["gpu"] = std::string(ToString(s.gpu));
        root["quality"] = std::string(ToString(s.quality));
        if (s.pendingFallback)
        {
            root["pendingFallback"] = std::string(ToString(*s.pendingFallback));
        }
        if (s.calibrateNextLaunch)
        {
            root["calibrateNextLaunch"] = true;
        }
        if (s.calibration)
        {
            const CalibrationRecord& c = *s.calibration;
            root["calibration"] = {
                { "version", c.version }, { "adapter", c.adapter }, { "backend", c.backend },
                { "width", c.width }, { "height", c.height }, { "msaa", c.msaaSamples },
                { "tier", std::string(ToString(c.tier)) }, { "medianMs", c.medianMs }, { "p95Ms", c.p95Ms },
            };
        }
        return root.dump(2) + "\n";
    }

    CommandLine ParseCommandLine(std::span<const std::string> args)
    {
        CommandLine line;
        for (std::size_t i = 0; i < args.size(); ++i)
        {
            const std::string& arg = args[i];
            const auto value = [&]() -> std::optional<std::string> {
                if (i + 1 < args.size() && args[i + 1].rfind("--", 0) != 0)
                {
                    return args[++i];
                }
                line.errors.push_back(arg + " needs a value");
                return std::nullopt;
            };
            if (arg == "--gpu")
            {
                if (auto text = value())
                {
                    line.gpu = ParseGpuPreference(*text);
                    if (!line.gpu) line.errors.push_back("--gpu: expected low-power or high-performance, got " + *text);
                }
            }
            else if (arg == "--quality")
            {
                if (auto text = value())
                {
                    line.quality = ParseQualityMode(*text);
                    if (!line.quality) line.errors.push_back("--quality: expected auto, low, balanced or high, got " + *text);
                }
            }
            else if (arg == "--diagnostics")
            {
                line.diagnosticsFile = value();
            }
            else if (arg == "--calibrate") line.calibrate = true;
            else if (arg == "--reset-settings") line.resetSettings = true;
            else if (arg == "--no-settings") line.noSettings = true;
            else line.errors.push_back("unknown argument " + arg);
        }
        return line;
    }

    ResolvedSettings ResolveSettings(const CommandLine& commandLine,
                                     const EnvironmentOverrides& environment,
                                     const GameSettings* saved)
    {
        ResolvedSettings resolved;
        resolved.gpuReason = "default: the stable choice on hybrid laptops";
        if (saved && saved->pendingFallback)
        {
            resolved.gpu = *saved->pendingFallback;
            resolved.gpuReason = "fallback: the high-performance adapter failed last time";
        }
        else if (saved)
        {
            resolved.gpu = saved->gpu;
            resolved.gpuReason = "saved setting";
        }
        if (environment.gpu)
        {
            resolved.gpu = *environment.gpu;
            resolved.gpuReason = "ATOM_GPU";
        }
        if (commandLine.gpu)
        {
            resolved.gpu = *commandLine.gpu;
            resolved.gpuReason = "--gpu";
        }

        if (saved) resolved.quality = saved->quality;
        if (environment.quality) resolved.quality = *environment.quality;
        if (commandLine.quality) resolved.quality = *commandLine.quality;
        return resolved;
    }

    QualityTier TierFor(QualityMode mode, const std::optional<CalibrationRecord>& calibration,
                        std::string_view adapter)
    {
        switch (mode)
        {
        case QualityMode::Low: return QualityTier::Low;
        case QualityMode::Balanced: return QualityTier::Balanced;
        case QualityMode::High: return QualityTier::High;
        case QualityMode::Auto: break;
        }
        if (calibration && IsCalibrationValid(*calibration, adapter))
        {
            return calibration->tier;
        }
        return QualityTier::High; // not calibrated (here): today's look
    }
}
