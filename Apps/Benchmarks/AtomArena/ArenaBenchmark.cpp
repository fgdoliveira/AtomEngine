#include "ArenaBenchmark.h"

#include <charconv>

namespace Arena
{
    namespace
    {
        // "collision_naive_500" -> ("collision_naive", 500); no number: nullopt.
        std::optional<std::pair<std::string, int>> Split(const std::string& name)
        {
            const std::size_t underscore = name.rfind('_');
            if (underscore == std::string::npos || underscore + 1 >= name.size())
            {
                return std::nullopt;
            }
            int count = 0;
            const char* first = name.data() + underscore + 1;
            const char* last = name.data() + name.size();
            const auto [end, error] = std::from_chars(first, last, count);
            if (error != std::errc{} || end != last || count < 0 || count > 20000)
            {
                return std::nullopt;
            }
            return std::make_pair(name.substr(0, underscore), count);
        }
    }

    std::optional<BenchmarkScenario> FindScenario(const std::string& name)
    {
        const auto parts = Split(name);
        if (!parts)
        {
            return std::nullopt;
        }
        const auto& [kind, count] = *parts;
        BenchmarkScenario s;
        s.name = name;
        ArenaConfig& c = s.config;
        c.projectiles = 0;
        if (kind == "update")
        {
            c.enemies = count;
            c.collide = false;
        }
        else if (kind == "collision" || kind == "collision_naive")
        {
            c.enemies = count;
            c.collision = kind == "collision" ? CollisionMode::Grid : CollisionMode::Naive;
        }
        else if (kind == "projectiles")
        {
            c.enemies = 250;
            c.projectiles = count;
        }
        else if (kind == "spike")
        {
            c.enemies = 250;
            s.spikeBatch = count;
        }
        else if (kind == "draw")
        {
            c.enemies = count;
            c.collide = false;
            s.drawEnemies = true;
        }
        else if (kind == "full")
        {
            c.enemies = count;
            c.projectiles = count * 2;
            s.drawEnemies = true;
        }
        else
        {
            return std::nullopt;
        }
        return s;
    }

    std::vector<std::string> StandardScenarios()
    {
        std::vector<std::string> names;
        for (const int n : { 50, 100, 250, 500, 1000 })
        {
            names.push_back("update_" + std::to_string(n));
            names.push_back("collision_" + std::to_string(n));
            names.push_back("collision_naive_" + std::to_string(n));
            names.push_back("draw_" + std::to_string(n));
        }
        for (const int n : { 100, 500, 1000, 2000 })
        {
            names.push_back("projectiles_" + std::to_string(n));
        }
        names.push_back("spike_500");
        names.push_back("full_250");
        return names;
    }

    BenchmarkOptions ParseBenchmarkArguments(const std::vector<std::string>& arguments)
    {
        BenchmarkOptions options;
        const auto number = [&](std::size_t& i, const std::string& flag, auto& into) {
            if (i + 1 >= arguments.size())
            {
                options.errors.push_back(flag + " needs a value");
                return;
            }
            const std::string& text = arguments[++i];
            const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), into);
            if (error != std::errc{} || end != text.data() + text.size())
            {
                options.errors.push_back(flag + ": not a number: " + text);
            }
        };
        for (std::size_t i = 0; i < arguments.size(); ++i)
        {
            const std::string& a = arguments[i];
            if (a == "--benchmark") options.benchmark = true;
            else if (a == "--scenario" && i + 1 < arguments.size()) options.scenario = arguments[++i];
            else if (a == "--out" && i + 1 < arguments.size()) options.out = arguments[++i];
            else if (a == "--seed") number(i, a, options.seed);
            else if (a == "--frames") number(i, a, options.frames);
            else if (a == "--warmup") number(i, a, options.warmup);
            else options.errors.push_back("unknown argument: " + a);
        }
        if (options.benchmark && !FindScenario(options.scenario))
        {
            options.errors.push_back("unknown scenario: " + options.scenario);
        }
        if (options.frames < 1 || options.warmup < 0)
        {
            options.errors.push_back("--frames must be positive and --warmup not negative");
        }
        return options;
    }
}
