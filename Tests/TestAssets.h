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
}
