#pragma once

#include "Platform/AssetRoots.h"

#include <string>

namespace AtomTests
{
    // The source tree's asset roots (v0.0.14), as the apps read them with
    // hot reload: the demo's Assets/, the Showcase's, then the shared
    // Content/. The tests read the sources, not the build's copy (CI builds
    // them without the apps, so there is no copy).
    inline const AtomFramework::AssetRoots& SourceAssets()
    {
        static const AtomFramework::AssetRoots roots(
            { ATOM_SOURCE_DIR "/Games/Demo/Assets/", ATOM_SOURCE_DIR "/Showcase/Assets/", ATOM_SOURCE_DIR "/Content/" });
        return roots;
    }

    // "Kit/road.glb" -> the file in whichever root has it.
    inline std::string Asset(const std::string& relative)
    {
        return SourceAssets().Resolve(relative);
    }

    // Every level file's folder: the demo's and the Showcase's.
    inline const char* const LevelFolders[] = { ATOM_SOURCE_DIR "/Games/Demo/Assets/Levels",
                                                ATOM_SOURCE_DIR "/Showcase/Assets/Levels" };

    // A JSON schema (editors and these tests only, never shipped): the
    // framework's formats (level, environment) in Framework/Schemas, the
    // demo's own (dialogue, flashlight, machine) beside its data (M85).
    inline std::string Schema(const std::string& name)
    {
        static const AtomFramework::AssetRoots roots({ ATOM_SOURCE_DIR "/Framework/Schemas/", ATOM_SOURCE_DIR "/Games/Demo/Assets/Schemas/" });
        return roots.Resolve(name);
    }

    // How a data file names the framework's schemas, from where it lives.
    inline constexpr const char* FrameworkSchemaFromDemo = "../../../../Framework/Schemas/";     // Games/Demo/Assets/<folder>/
    inline constexpr const char* FrameworkSchemaFromShowcase = "../../../Framework/Schemas/";    // Showcase/Assets/<folder>/
    inline constexpr const char* FrameworkSchemaFromContent = "../../Framework/Schemas/";        // Content/<folder>/
}
