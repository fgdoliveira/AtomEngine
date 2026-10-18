#include "Platform/AssetRoots.h"

#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>

using namespace AtomFramework;

namespace
{
    void Touch(const std::filesystem::path& path)
    {
        std::filesystem::create_directories(path.parent_path());
        std::ofstream(path) << "x";
    }
}

TEST_CASE("AssetRoots lists a folder's files across its roots: by extension, sorted, each once")
{
    const std::filesystem::path base = std::filesystem::temp_directory_path() / "atom_asset_roots_test";
    std::filesystem::remove_all(base);
    Touch(base / "app" / "Sky" / "night.png");
    Touch(base / "app" / "Sky" / "notes.txt");
    Touch(base / "content" / "Sky" / "dawn.png");
    Touch(base / "content" / "Sky" / "night.png"); // in both: listed once
    std::filesystem::create_directories(base / "content" / "Sky" / "folder.png"); // a folder, not a file

    const AssetRoots roots({ (base / "app").string(), (base / "content").string() });
    const std::vector<std::string> skies = roots.List("Sky", ".png");
    REQUIRE(skies.size() == 2);
    CHECK(skies[0] == "dawn.png");
    CHECK(skies[1] == "night.png");
    CHECK(roots.List("Nowhere", ".png").empty()); // a missing folder is no error

    std::filesystem::remove_all(base);
}
