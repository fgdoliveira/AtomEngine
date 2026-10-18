#include "RecentProjects.h"

#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <string>

using AtomWorkspace::RecentProjects;
namespace fs = std::filesystem;

TEST_CASE("Recent projects: newest first, no duplicates, at most MaxEntries")
{
    RecentProjects recents;
    recents.Add("D:/Games/A/A.atomproject.json");
    recents.Add("D:/Games/B/B.atomproject.json");
    recents.Add("D:/Games/A/A.atomproject.json"); // opened again: back to the front
    REQUIRE(recents.Entries().size() == 2);
    CHECK(recents.Entries()[0] == "D:/Games/A/A.atomproject.json");
    CHECK(recents.Entries()[1] == "D:/Games/B/B.atomproject.json");

    recents.Add("");
    CHECK(recents.Entries().size() == 2);

    for (int i = 0; i < 20; ++i)
    {
        recents.Add("D:/Games/P" + std::to_string(i) + "/P" + std::to_string(i) + ".atomproject.json");
    }
    REQUIRE(recents.Entries().size() == RecentProjects::MaxEntries);
    CHECK(recents.Entries()[0] == "D:/Games/P19/P19.atomproject.json");

    recents.Remove("D:/Games/P19/P19.atomproject.json");
    CHECK(recents.Entries()[0] == "D:/Games/P18/P18.atomproject.json");
}

TEST_CASE("Recent projects: saved and loaded, paths with spaces, CRLF and blank lines tolerated")
{
    const fs::path folder = fs::temp_directory_path() / "atom workspace tests";
    fs::create_directories(folder);
    const std::string file = (folder / "recent.txt").string();

    RecentProjects saved;
    saved.Add("D:/My Games/Old/Old.atomproject.json");
    saved.Add("D:/My Games/New/New.atomproject.json");
    REQUIRE(saved.Save(file));

    RecentProjects loaded;
    loaded.Load(file);
    CHECK(loaded.Entries() == saved.Entries());

    {
        std::ofstream edited(file, std::ios::binary | std::ios::trunc);
        edited << "D:/X/X.atomproject.json\r\n\r\nD:/X/X.atomproject.json\r\nD:/Y/Y.atomproject.json\n";
    }
    loaded.Load(file);
    REQUIRE(loaded.Entries().size() == 2);
    CHECK(loaded.Entries()[1] == "D:/Y/Y.atomproject.json");

    loaded.Load((folder / "missing.txt").string()); // a first run: no file, no entries
    CHECK(loaded.Entries().empty());

    std::error_code ignored;
    fs::remove_all(folder, ignored);
}
