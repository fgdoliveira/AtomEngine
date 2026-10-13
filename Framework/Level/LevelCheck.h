#pragma once

#include "Platform/AssetRoots.h"

#include <string>
#include <vector>

namespace AtomFramework
{
    // Content validation without the game (v0.0.17, M112): a level file
    // checked the way the app would load it, plus what only shows across
    // files - referenced models, collisions, lightmaps and clips that
    // exist, spawns a level change lands on, entity names that are unique.
    // The parser (ParseLevel) stays the authority for what a level file
    // may say; this adds the checks that need other files. Used by
    // Tools/LevelCheck (AtomLevelCheck), the asset export and the content
    // tests. No window, no GPU.
    struct LevelDiagnostic
    {
        enum class Severity
        {
            Error,   // the level would fail to load, or fail when played
            Warning, // supported, but probably not what was meant
        };
        Severity severity = Severity::Error;
        std::string level;    // the level's file name without .json
        std::string source;   // the file the problem is in, relative to the asset root ("Levels/street.json")
        std::string entity;   // the entity or spawn it is about, if any
        std::string property; // its JSON Pointer, if any ("/entities/12/model")
        std::string problem;
        std::string fix;      // a suggestion, if there is one
    };

    // Another app's asset folder: named when a missing file is only there.
    struct OtherAppAssets
    {
        std::string app;    // "showcase"
        std::string folder; // its Assets/
    };

    struct LevelCheckOptions
    {
        AssetRoots assets;                         // the app's own Assets/, then the shared Content/
        std::vector<OtherAppAssets> otherApps;     // named when a missing file is only theirs
        bool checkClips = true;                    // open the models (.glb): readable, and their animation clips
    };

    // Checks <root>/Levels/<level>.json. `levelPath` may also be a full path.
    std::vector<LevelDiagnostic> CheckLevelFile(const std::string& levelPath, const LevelCheckOptions& options);

    // Every Levels/*.json under the app's first asset root (markers are
    // read with their level).
    std::vector<LevelDiagnostic> CheckLevels(const LevelCheckOptions& options);

    // The report a person or an agent reads:
    //   ERROR [street]
    //     Entity: lamp_09
    //     Source: Levels/street.json
    //     Property: /entities/12/model
    //     Problem: model "Kit/lamp.glb" was not found
    //     Fix: ...
    std::string FormatDiagnostic(const LevelDiagnostic& diagnostic);

    std::size_t CountErrors(const std::vector<LevelDiagnostic>& diagnostics);
}
