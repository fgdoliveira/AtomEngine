#pragma once

#include <glm/vec3.hpp>

#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace AtomFramework
{
    // A game's progress between runs (demo vertical): written at a save
    // point, read by the title screen's Continue. One file, save.json, under
    // the app's pref path beside settings.json. Pure: parsing and writing
    // are tested without a game; the app decides when and where.
    struct SaveGame
    {
        static constexpr int SchemaVersion = 1;

        std::string level;              // where it was saved
        std::string spawn;              // the level's spawn, if the position can't be used
        glm::vec3 position{ 0.0f };     // the player's feet
        float yawRadians = 0.0f;
        float pitchRadians = 0.0f;
        std::vector<std::string> flags; // sorted
        std::map<std::string, int> counters;
        bool flashlightOn = false;
        bool headlightOn = true;
        std::string place;              // the save point's name, for the title ("the roadside shrine")
        std::string savedAt;            // local time, "2026-10-18 21:05"
    };

    struct SaveLoad
    {
        bool found = false;  // a save was read (Continue is offered)
        SaveGame save;
        std::string warning; // "" when the file was read as is
    };

    // A broken, truncated or newer file is a warning and no save - never a
    // crash, never a guess. Unknown keys are ignored (an older build reading
    // a newer minor addition).
    SaveLoad ParseSave(std::string_view json);
    // Sorted and stable: the same progress writes the same bytes.
    std::string WriteSave(const SaveGame& save);

    // Writes `text` beside `path` first, then replaces `path` with it: a
    // crash or power cut mid-write leaves the old file, not half of the new.
    bool WriteFileAtomically(const std::string& path, std::string_view text);

    // The pref-path file for an app ("" if there's no pref path).
    std::string SavePathFor(std::string_view app);
}
