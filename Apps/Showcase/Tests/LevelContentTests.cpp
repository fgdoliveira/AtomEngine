#include "LevelChecks.h"
#include "TestAssets.h"

#include <doctest/doctest.h>
#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <string>

// Shipped files checked: the "content" suite (Content.<owner> in ctest).
TEST_SUITE_BEGIN("content");

TEST_CASE("Every Showcase level file is valid")
{
    CHECK(AtomTests::CheckShippedLevels() >= 3);
}

TEST_CASE("The Showcase's levels declare their schema, which knows their keys")
{
    std::ifstream schemaFile(AtomTests::Schema("level.schema.json"));
    const nlohmann::json keys = nlohmann::json::parse(schemaFile)["properties"];
    const std::string schema = std::string(AtomTests::FrameworkSchemaFromShowcase) + "level.schema.json";
    int files = 0;
    for (const auto& entry : std::filesystem::directory_iterator(AtomTests::Asset("Levels")))
    {
        const std::string name = entry.path().filename().string();
        if (entry.path().extension() != ".json" || name.ends_with(".markers.json"))
        {
            continue;
        }
        ++files;
        INFO(name);
        std::ifstream file(entry.path());
        const nlohmann::json root = nlohmann::json::parse(file);
        CHECK(root.value("$schema", "") == schema);
        for (const auto& [key, value] : root.items())
        {
            INFO(key);
            CHECK(keys.contains(key)); // a key the schema doesn't know would be flagged in the editor
        }
    }
    CHECK(files >= 3);
}

TEST_SUITE_END();
