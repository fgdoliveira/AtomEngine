#include "Environment/EnvironmentPresets.h"

#include "Core/AssetLog.h"
#include "Level/Level.h"

#include <filesystem>
#include <fstream>
#include <iterator>

namespace AtomFramework
{
    std::vector<std::string> EnvironmentPresets::Load(const std::string& folder)
    {
        m_presets.clear();
        std::vector<std::string> problems;
        std::error_code error;
        for (const auto& entry : std::filesystem::directory_iterator(folder, error))
        {
            if (entry.path().extension() != ".json")
            {
                continue;
            }
            Atom::AssetLog::Opened(entry.path().string());
            std::ifstream file(entry.path(), std::ios::binary);
            std::string text{ std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>() };
            EnvironmentState probe;
            if (const std::string problem = ApplyEnvironmentPreset(text, probe); !problem.empty())
            {
                problems.push_back(entry.path().filename().string() + ": " + problem);
                continue;
            }
            m_presets[entry.path().stem().string()] = std::move(text);
        }
        return problems;
    }

    EnvironmentState EnvironmentPresets::Resolve(const Level* level, const std::string& name) const
    {
        EnvironmentState state = level ? static_cast<const EnvironmentState&>(level->GetData().lighting)
                                       : EnvironmentState{};
        if (const auto found = m_presets.find(name); found != m_presets.end())
        {
            ApplyEnvironmentPreset(found->second, state); // checked when loaded
        }
        return state;
    }

    std::vector<std::string> EnvironmentPresets::Offered(const Level* level) const
    {
        if (level && level->GetData().environment && !level->GetData().environment->presets.empty())
        {
            return level->GetData().environment->presets;
        }
        std::vector<std::string> names;
        for (const auto& [name, text] : m_presets)
        {
            names.push_back(name);
        }
        return names;
    }
}
