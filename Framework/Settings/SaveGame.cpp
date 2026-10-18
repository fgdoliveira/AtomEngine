#include "Settings/SaveGame.h"

#include <SDL3/SDL.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <system_error>

namespace AtomFramework
{
    namespace
    {
        using Json = nlohmann::json;

        bool Finite(const glm::vec3& v) { return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z); }
    }

    SaveLoad ParseSave(std::string_view text)
    {
        SaveLoad result;
        if (text.empty())
        {
            return result; // no save yet: nothing to say
        }
        const Json root = Json::parse(text, nullptr, false);
        if (root.is_discarded() || !root.is_object())
        {
            result.warning = "save file is not valid JSON; no save";
            return result;
        }
        const int schema = root.value("schema", 0);
        if (schema != SaveGame::SchemaVersion)
        {
            result.warning = "save file has schema " + std::to_string(schema) + ", expected "
                + std::to_string(SaveGame::SchemaVersion) + "; not loaded";
            return result;
        }
        SaveGame& s = result.save;
        s.level = root.value("level", std::string{});
        if (s.level.empty())
        {
            result.warning = "save file names no level; not loaded";
            return result;
        }
        s.spawn = root.value("spawn", std::string{});
        if (const auto p = root.find("position"); p != root.end() && p->is_array() && p->size() == 3
            && (*p)[0].is_number() && (*p)[1].is_number() && (*p)[2].is_number())
        {
            const glm::vec3 position{ (*p)[0].get<float>(), (*p)[1].get<float>(), (*p)[2].get<float>() };
            if (Finite(position))
            {
                s.position = position;
            }
        }
        s.yawRadians = root.value("yaw", 0.0f);
        s.pitchRadians = std::clamp(root.value("pitch", 0.0f), -1.5f, 1.5f);
        if (const auto flags = root.find("flags"); flags != root.end() && flags->is_array())
        {
            for (const Json& flag : *flags)
            {
                if (flag.is_string())
                {
                    s.flags.push_back(flag.get<std::string>());
                }
            }
            std::sort(s.flags.begin(), s.flags.end());
            s.flags.erase(std::unique(s.flags.begin(), s.flags.end()), s.flags.end());
        }
        if (const auto counters = root.find("counters"); counters != root.end() && counters->is_object())
        {
            for (const auto& [name, value] : counters->items())
            {
                if (value.is_number_integer())
                {
                    s.counters[name] = std::max(0, value.get<int>());
                }
            }
        }
        s.flashlightOn = root.value("flashlightOn", false);
        s.headlightOn = root.value("headlightOn", true);
        s.place = root.value("place", std::string{});
        s.savedAt = root.value("savedAt", std::string{});
        result.found = true;
        return result;
    }

    std::string WriteSave(const SaveGame& s)
    {
        // nlohmann's default object is sorted by key: stable bytes.
        Json root;
        root["schema"] = SaveGame::SchemaVersion;
        root["level"] = s.level;
        root["spawn"] = s.spawn;
        root["position"] = { s.position.x, s.position.y, s.position.z };
        root["yaw"] = s.yawRadians;
        root["pitch"] = s.pitchRadians;
        std::vector<std::string> flags = s.flags;
        std::sort(flags.begin(), flags.end());
        flags.erase(std::unique(flags.begin(), flags.end()), flags.end());
        root["flags"] = flags;
        root["counters"] = Json::object();
        for (const auto& [name, value] : s.counters)
        {
            root["counters"][name] = value;
        }
        root["flashlightOn"] = s.flashlightOn;
        root["headlightOn"] = s.headlightOn;
        root["place"] = s.place;
        root["savedAt"] = s.savedAt;
        return root.dump(2) + "\n";
    }

    bool WriteFileAtomically(const std::string& path, std::string_view text)
    {
        const std::string temporary = path + ".tmp";
        {
            std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
            file.write(text.data(), static_cast<std::streamsize>(text.size()));
            file.flush();
            if (!file)
            {
                return false;
            }
        }
        std::error_code error;
        std::filesystem::rename(temporary, path, error); // replaces on Windows and POSIX
        if (error)
        {
            std::filesystem::remove(temporary, error);
            return false;
        }
        return true;
    }

    std::string SavePathFor(std::string_view app)
    {
        std::string path;
        if (char* pref = SDL_GetPrefPath("AtomEngine", std::string(app).c_str()))
        {
            path = std::string(pref) + "save.json";
            SDL_free(pref);
        }
        return path;
    }
}
