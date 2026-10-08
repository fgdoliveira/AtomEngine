#include "Environment/EnvironmentController.h"
#include "TestAssets.h" // v0.0.14: source assets in two roots
#include "Level/LevelData.h"

#include <doctest/doctest.h>
#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

using namespace AtomFramework; // v0.0.14: the world layer

namespace
{
    EnvironmentState Bright()
    {
        EnvironmentState s;
        s.sunColor = { 1.0f, 1.0f, 1.0f };
        s.fogDensity = 0.0f;
        return s;
    }

    EnvironmentState Dark()
    {
        EnvironmentState s;
        s.sunColor = { 0.0f, 0.0f, 0.0f };
        s.fogDensity = 0.1f;
        return s;
    }
}

TEST_CASE("A switch with no time is immediate; with time it blends, eased, to exactly the target")
{
    EnvironmentController controller;
    controller.Reset(Bright());
    CHECK_FALSE(controller.IsTransitioning());
    CHECK(controller.Progress() == doctest::Approx(1.0f));

    controller.SwitchTo(Dark(), 0.0f);
    CHECK(controller.Current().sunColor.x == doctest::Approx(0.0f));

    controller.Reset(Bright());
    controller.SwitchTo(Dark(), 2.0f);
    CHECK(controller.IsTransitioning());
    CHECK(controller.Current().sunColor.x == doctest::Approx(1.0f)); // nothing moves before time passes
    controller.Update(1.0f);
    CHECK(controller.Progress() == doctest::Approx(0.5f));
    CHECK(controller.Current().sunColor.x == doctest::Approx(0.5f)); // eased: the middle is the middle
    controller.Update(0.5f);
    CHECK(controller.Current().sunColor.x < 0.25f); // past the middle, easing toward the end
    controller.Update(10.0f);
    CHECK_FALSE(controller.IsTransitioning());
    CHECK(controller.Current().sunColor.x == doctest::Approx(0.0f));
    CHECK(*controller.Current().fogDensity == doctest::Approx(0.1f));
}

TEST_CASE("Changing your mind mid-blend starts from what's showing, and frame rate doesn't matter")
{
    EnvironmentController controller;
    controller.Reset(Bright());
    controller.SwitchTo(Dark(), 2.0f);
    controller.Update(1.0f);
    controller.SwitchTo(Bright(), 2.0f);
    CHECK(controller.Current().sunColor.x == doctest::Approx(0.5f)); // no jump

    // The same time in big or small steps lands on the same state.
    EnvironmentController coarse;
    EnvironmentController fine;
    coarse.Reset(Bright());
    fine.Reset(Bright());
    coarse.SwitchTo(Dark(), 3.0f);
    fine.SwitchTo(Dark(), 3.0f);
    coarse.Update(1.2f);
    for (int i = 0; i < 12; ++i)
    {
        fine.Update(0.1f);
    }
    CHECK(coarse.Current().sunColor.x == doctest::Approx(fine.Current().sunColor.x));
}

TEST_CASE("A preset changes only what it names, and a broken one changes nothing")
{
    EnvironmentState state;
    state.sunColor = { 0.9f, 0.9f, 0.9f };
    state.water.ripple = 1.0f;
    CHECK(ApplyEnvironmentPreset(R"({ "name": "rain", "fogDensity": 0.03, "water": { "ripple": 2, "glint": 0.2 } })", state).empty());
    CHECK(state.water.glint == doctest::Approx(0.2f));
    CHECK(*state.fogDensity == doctest::Approx(0.03f));
    CHECK(state.water.ripple == doctest::Approx(2.0f));
    CHECK(state.sunColor.x == doctest::Approx(0.9f)); // left as it was
    CHECK(state.water.skyReflection == doctest::Approx(WaterLook{}.skyReflection));

    // A sky gradient in a preset changes only the parts it gives.
    state.sky = SkyGradient{};
    CHECK(ApplyEnvironmentPreset(R"({ "skyGradient": { "sunGlow": 1.5 } })", state).empty());
    CHECK(state.sky->sunGlow == doctest::Approx(1.5f));
    CHECK(state.sky->sunSize == doctest::Approx(SkyGradient{}.sunSize));

    const EnvironmentState before = state;
    const std::string error = ApplyEnvironmentPreset(R"({ "fogDensity": 0.5, "water": { "skyReflection": 3 } })", state);
    CHECK(error.rfind("/water", 0) == 0);
    CHECK(*state.fogDensity == doctest::Approx(*before.fogDensity)); // all or nothing
    CHECK_FALSE(ApplyEnvironmentPreset("[1, 2]", state).empty());
    CHECK_FALSE(ApplyEnvironmentPreset("{ broken", state).empty());
}

TEST_CASE("A level's environment block parses and its default must be offered")
{
    const std::string base = R"({ "name": "x", "model": "m", "collision": "c",
        "spawns": { "a": { "position": [0,0,0] } }, )";
    const auto ok = ParseLevel(base + R"("environment": { "default": "rain", "presets": ["clear_day", "rain"] } })");
    INFO(ok.error);
    REQUIRE(ok.level.has_value());
    REQUIRE(ok.level->environment.has_value());
    CHECK(ok.level->environment->defaultPreset == "rain");
    CHECK(ok.level->environment->presets.size() == 2);

    CHECK_FALSE(ParseLevel(base + R"("environment": { "default": "snow", "presets": ["rain"] } })").level.has_value());
    CHECK_FALSE(ParseLevel(base + R"("environment": { "presets": ["rain"] } })").level.has_value());
    CHECK(ParseLevel(base + R"("environment": { "default": "fog" } })").level.has_value()); // any preset
}

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
        CHECK(root.value("$schema", "") == std::string(AtomTests::FrameworkSchemaFromDemo) + "environment.schema.json");
        for (const auto& [key, value] : root.items())
        {
            CHECK(keys.contains(key));
        }
    }
    CHECK(count >= 6);
}
