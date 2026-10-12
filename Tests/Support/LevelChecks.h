#pragma once

#include "Level/LevelData.h"
#include "TestAssets.h"

#include <doctest/doctest.h>

#include <filesystem>

namespace AtomTests
{
    // Every level file the owner ships (its Assets/Levels/) parses, and the
    // files it names exist where the app looks: its own assets, then the
    // shared Content (a level naming a missing lightmap would fail to load
    // in the game). Returns how many were checked.
    inline int CheckShippedLevels()
    {
        int count = 0;
        for (const auto& entry : std::filesystem::directory_iterator(Asset("Levels")))
        {
            if (entry.path().extension() != ".json" || entry.path().filename().string().ends_with(".markers.json"))
            {
                continue; // markers are read with their level
            }
            const AtomFramework::LevelParseResult result = AtomFramework::LoadLevelFile(entry.path().string());
            INFO(entry.path().filename().string() << ": " << result.error);
            CHECK(result.level.has_value());
            if (result.level)
            {
                CHECK(std::filesystem::exists(Asset(result.level->model)));
                CHECK(std::filesystem::exists(Asset(result.level->collision)));
                if (result.level->lightmap)
                {
                    CHECK(std::filesystem::exists(Asset(result.level->lightmap->texture)));
                }
            }
            ++count;
        }
        return count;
    }
}
