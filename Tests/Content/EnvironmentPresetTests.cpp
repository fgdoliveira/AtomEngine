#include "Environment/EnvironmentController.h"
#include "Level/LevelData.h"
#include "TestAssets.h"

#include <doctest/doctest.h>
#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

using namespace AtomFramework;

// Shipped files checked: the "content" suite (Content.<owner> in ctest).
TEST_SUITE_BEGIN("content");

TEST_CASE("Every shipped preset is valid, declares its schema, and the schema knows its keys")
{
    const std::string folder = AtomTests::Asset("Environments");
    std::ifstream schemaFile(AtomTests::Schema("environment.schema.json"));
    const nlohmann::json keys = nlohmann::json::parse(schemaFile)["properties"];
    int count = 0;
    for (const auto& entry : std::filesystem::directory_iterator(folder))
    {
        if (entry.path().extension() != ".json")
        {
            continue;
        }
        ++count;
        INFO(entry.path().string());
        std::ifstream file(entry.path(), std::ios::binary);
        const std::string text{ std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>() };
        EnvironmentState state;
        CHECK(ApplyEnvironmentPreset(text, state) == "");
        const nlohmann::json root = nlohmann::json::parse(text);
        CHECK(root.value("$schema", "") == std::string(AtomTests::FrameworkSchemaFromContent) + "environment.schema.json");
        for (const auto& [key, value] : root.items())
        {
            CHECK(keys.contains(key));
        }
    }
    CHECK(count >= 6);
}

TEST_SUITE_END();
