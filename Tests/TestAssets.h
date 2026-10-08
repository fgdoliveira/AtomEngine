#pragma once

#include "Platform/AssetRoots.h"

#include <string>

namespace AtomTests
{
    // The source tree's asset roots (v0.0.14), as the demo reads them with
    // hot reload: its own Assets/ first, then the shared Content/. The tests
    // read the sources, not the build's copy (CI builds them without the
    // game, so there is no copy).
    inline const AtomFramework::AssetRoots& SourceAssets()
    {
        static const AtomFramework::AssetRoots roots({ ATOM_SOURCE_DIR "/Games/Demo/Assets/", ATOM_SOURCE_DIR "/Content/" });
        return roots;
    }

    // "Kit/road.glb" -> the file in whichever root has it.
    inline std::string Asset(const std::string& relative)
    {
        return SourceAssets().Resolve(relative);
    }

    // A JSON schema (editors and these tests only, never shipped): the
    // framework's formats (level, environment) in Framework/Schemas, the
    // demo's own (dialogue, flashlight, machine) beside its data (M85).
    inline std::string Schema(const std::string& name)
    {
        static const AtomFramework::AssetRoots roots({ ATOM_SOURCE_DIR "/Framework/Schemas/", ATOM_SOURCE_DIR "/Games/Demo/Assets/Schemas/" });
        return roots.Resolve(name);
    }

    // How a demo data file names a framework schema: from Games/Demo/Assets/<folder>/.
    inline constexpr const char* FrameworkSchemaFromDemo = "../../../../Framework/Schemas/";
}
