// The demo's authored files: dialogue errors, the schemas its levels,
// dialogues and machines declare, and its committed lightmaps. The level
// format's own errors: Tests/Framework/AuthoringTests.cpp.
#include "Dialogue/Dialogue.h"
#include "TestAssets.h"

#include <doctest/doctest.h>
#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <tuple>

using namespace Demo;

namespace
{
    nlohmann::json ReadJson(const std::string& path)
    {
        std::ifstream file(path);
        return nlohmann::json::parse(file);
    }
}

TEST_CASE("Dialogue errors say where: the line and column, or the JSON path")
{
    const auto syntax = ParseDialogue("{ \"id\": \"a\",\n\n  \"start\": }");
    INFO(syntax.error);
    CHECK(syntax.error.rfind("line 3, column", 0) == 0);

    const auto link = ParseDialogue(R"({ "id": "d", "start": "a", "nodes": [
        { "id": "a", "text": "hi", "next": "b" },
        { "id": "b", "text": "?", "choices": [ { "text": "ok", "next": "end" }, { "text": "x", "next": "typo" } ] } ] })");
    INFO(link.error);
    CHECK(link.error.rfind("/nodes/1/choices/1/next:", 0) == 0);
}

// Shipped files checked: the "content" suite (Content.<owner> in ctest).
TEST_SUITE_BEGIN("content");

TEST_CASE("The demo's levels, dialogues and machines declare their schema, which knows their keys")
{
    const auto properties = [](const std::string& schema) {
        return ReadJson(AtomTests::Schema(schema))["properties"];
    };
    const nlohmann::json level = properties("level.schema.json");
    const nlohmann::json dialogue = properties("dialogue.schema.json");
    const nlohmann::json machine = properties("machine.schema.json");
    const std::string levelSchema = std::string(AtomTests::FrameworkSchemaFromDemo) + "level.schema.json";

    int files = 0;
    for (const auto& [folder, schema, keys] : {
             std::tuple{ AtomTests::Asset("Levels"), levelSchema.c_str(), &level },
             std::tuple{ AtomTests::Asset("Dialogue"), "../Schemas/dialogue.schema.json", &dialogue },
             std::tuple{ AtomTests::Asset("Machines"), "../Schemas/machine.schema.json", &machine } })
    {
        for (const auto& entry : std::filesystem::directory_iterator(folder))
        {
            const std::string name = entry.path().filename().string();
            if (entry.path().extension() != ".json" || name.ends_with(".markers.json"))
            {
                continue;
            }
            ++files;
            INFO(name);
            const nlohmann::json root = ReadJson(entry.path().string());
            CHECK(root.value("$schema", "") == schema);
            for (const auto& [key, value] : root.items())
            {
                INFO(key);
                CHECK(keys->contains(key)); // a key the schema doesn't know would be flagged in the editor
            }
        }
    }
    CHECK(files >= 9);
}

TEST_CASE("Committed lightmaps were baked on the CPU (byte-identical builds)")
{
    // build_assets --gpu marks its lightmaps: fast to iterate with, but not
    // reproducible, so they must be rebuilt on the CPU before committing.
    int lightmaps = 0;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(AtomTests::OwnAssets))
    {
        const std::string name = entry.path().filename().string();
        if (!name.ends_with("_lm.png"))
        {
            continue;
        }
        ++lightmaps;
        std::ifstream file(entry.path(), std::ios::binary);
        const std::string bytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        INFO(name);
        CHECK(bytes.find("atom-gpu-bake") == std::string::npos);
    }
    CHECK(lightmaps >= 6);
}

TEST_SUITE_END();
