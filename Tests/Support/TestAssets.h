#pragma once

#include <filesystem>
#include <string>

// Where a test target's files are (v0.0.16, M105-M106). Each test target
// has an owner, and sees only what that owner ships:
//   ATOM_TEST_ASSETS  the owner's own asset folder ("" for the engine,
//                     the framework and the shared content), then the
//                     shared Content/ - never another app's.
//   ATOM_SOURCE_DIR   the repository, for the framework's schemas.
// The tests read the sources, not a build's copy (CI builds without the
// apps, so there is no copy). Plain std::filesystem: the engine's tests
// don't depend on the framework's asset resolver.
#ifndef ATOM_TEST_ASSETS
#define ATOM_TEST_ASSETS ""
#endif

namespace AtomTests
{
    inline constexpr const char* OwnAssets = ATOM_TEST_ASSETS;
    inline constexpr const char* SharedContent = ATOM_SOURCE_DIR "/Content/";

    // "Kit/road.glb" -> the owner's file if it has one, else the shared
    // Content's. Found nowhere: the Content path, which doesn't exist.
    inline std::string Asset(const std::string& relative)
    {
        const std::string own = std::string(OwnAssets) + relative;
        if (*OwnAssets != '\0' && std::filesystem::exists(own))
        {
            return own;
        }
        return SharedContent + relative;
    }

    // A JSON schema (editors and these tests only, never shipped): the
    // owner's own (Assets/Schemas/), else the framework's (level, environment).
    inline std::string Schema(const std::string& name)
    {
        const std::string own = std::string(OwnAssets) + "Schemas/" + name;
        if (*OwnAssets != '\0' && std::filesystem::exists(own))
        {
            return own;
        }
        return ATOM_SOURCE_DIR "/Framework/Schemas/" + name;
    }

    // How a data file names the framework's schemas, from where it lives.
    inline constexpr const char* FrameworkSchemaFromDemo = "../../../../../Framework/Schemas/";  // Apps/Games/Demo/Assets/<folder>/
    inline constexpr const char* FrameworkSchemaFromShowcase = "../../../../Framework/Schemas/"; // Apps/Showcase/Assets/<folder>/
    inline constexpr const char* FrameworkSchemaFromContent = "../../Framework/Schemas/";        // Content/<folder>/
}
