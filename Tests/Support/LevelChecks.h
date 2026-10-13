#pragma once

#include "Level/LevelCheck.h"
#include "TestAssets.h"

#include <doctest/doctest.h>

#include <filesystem>

namespace AtomTests
{
    // Every level file the owner ships (its Assets/Levels/) passes the
    // framework's level check (M112, the same one as AtomLevelCheck): it
    // parses, and the models, collisions, lightmaps and clips it names
    // exist where that app looks - its own assets, then the shared
    // Content. Warnings don't fail it. Returns how many levels were read.
    inline int CheckShippedLevels()
    {
        AtomFramework::LevelCheckOptions options;
        options.assets = AtomFramework::AssetRoots({ OwnAssets, SharedContent });
        for (const AtomFramework::LevelDiagnostic& diagnostic : AtomFramework::CheckLevels(options))
        {
            INFO(AtomFramework::FormatDiagnostic(diagnostic));
            CHECK(diagnostic.severity != AtomFramework::LevelDiagnostic::Severity::Error);
        }
        int count = 0;
        for (const auto& entry : std::filesystem::directory_iterator(Asset("Levels")))
        {
            const std::string name = entry.path().filename().string();
            count += entry.path().extension() == ".json" && !name.ends_with(".markers.json") ? 1 : 0;
        }
        return count;
    }
}
