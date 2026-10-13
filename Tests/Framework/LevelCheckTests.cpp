#include "Level/LevelCheck.h"

#include <doctest/doctest.h>

#include <algorithm>
#include <string>
#include <vector>

using namespace AtomFramework;

namespace
{
    // Fixture levels (M112): good.json, broken.json (one of each mistake),
    // overridden.json (markers the file overrides), syntax.json. Models are
    // placeholders: only their existence is checked here (clips: the
    // content tests, which have real models).
    const std::string Fixtures = ATOM_SOURCE_DIR "/Tests/Framework/Fixtures/LevelCheck/";

    LevelCheckOptions Options()
    {
        LevelCheckOptions options;
        options.assets = AssetRoots({ Fixtures + "Assets/" });
        options.otherApps = { { "other", Fixtures + "OtherApp/" } };
        options.checkClips = false;
        return options;
    }

    const LevelDiagnostic* Find(const std::vector<LevelDiagnostic>& found, const std::string& property)
    {
        const auto it = std::find_if(found.begin(), found.end(),
            [&](const LevelDiagnostic& d) { return d.property == property; });
        return it == found.end() ? nullptr : &*it;
    }

    bool Says(const LevelDiagnostic* d, const std::string& words)
    {
        return d && d->problem.find(words) != std::string::npos;
    }
}

TEST_CASE("A valid level has nothing to report, its level change included")
{
    const std::vector<LevelDiagnostic> found = CheckLevelFile("good", Options());
    for (const LevelDiagnostic& d : found)
    {
        INFO(FormatDiagnostic(d));
    }
    CHECK(found.empty());
}

TEST_CASE("Each mistake in a level is an error at its entity and JSON Pointer")
{
    const std::vector<LevelDiagnostic> found = CheckLevelFile("broken", Options());

    const LevelDiagnostic* missing = Find(found, "/entities/0/model");
    REQUIRE(Says(missing, "\"Kit/missing_lamp.glb\" was not found"));
    CHECK(missing->entity == "lamp");
    CHECK(missing->source == "Levels/broken.json");
    CHECK(missing->level == "broken");
    CHECK(missing->severity == LevelDiagnostic::Severity::Error);

    CHECK(Says(Find(found, "/entities/1/name"), "already named \"lamp\""));
    const LevelDiagnostic* foreign = Find(found, "/entities/2/model");
    CHECK(Says(foreign, "only in another app's assets (other)"));
    CHECK(Says(Find(found, "/entities/3/collider/halfExtents"), "must all be positive"));
    const LevelDiagnostic* far = Find(found, "/entities/4/position");
    REQUIRE(far);
    CHECK(far->severity == LevelDiagnostic::Severity::Warning);
    CHECK(Says(Find(found, "/entities/5/interactable/action"), "no Levels/nowhere.json"));
    CHECK(Says(Find(found, "/entities/6/interactable/action"), "spawn \"nope\", which good doesn't have"));
    CHECK(Says(Find(found, "/entities/7/interactable/action"), "entity \"ghost\""));
    CHECK(CountErrors(found) == 7);
}

TEST_CASE("A marker the level file overrides is a warning, not an error")
{
    const std::vector<LevelDiagnostic> found = CheckLevelFile("overridden", Options());
    REQUIRE(found.size() == 1);
    CHECK(found[0].severity == LevelDiagnostic::Severity::Warning);
    CHECK(found[0].entity == "statue");
    CHECK(found[0].property == "/entities/0/position");
    CHECK(Says(&found[0], "entity:statue marker"));
    CHECK(CountErrors(found) == 0);
}

TEST_CASE("A syntax error names the file, line and column")
{
    const std::vector<LevelDiagnostic> found = CheckLevelFile("syntax", Options());
    REQUIRE(found.size() == 1);
    CHECK(found[0].source == "Levels/syntax.json");
    CHECK(found[0].problem.find("line 4") != std::string::npos);
}

TEST_CASE("The report reads as the authoring guide shows it")
{
    const LevelDiagnostic d{ LevelDiagnostic::Severity::Error, "lakeshore", "Levels/lakeshore.json", "lamp_09",
                             "/entities/12/model", "model \"Kit/lamp.glb\" was not found", "check the path" };
    CHECK(FormatDiagnostic(d) == "ERROR [lakeshore]\n  Entity: lamp_09\n  Source: Levels/lakeshore.json\n"
                                 "  Property: /entities/12/model\n  Problem: model \"Kit/lamp.glb\" was not found\n"
                                 "  Fix: check the path\n");
}

TEST_CASE("Checking a whole app's Levels/ reads every level, markers with theirs")
{
    const std::vector<LevelDiagnostic> found = CheckLevels(Options());
    std::vector<std::string> levels;
    for (const LevelDiagnostic& d : found)
    {
        if (std::find(levels.begin(), levels.end(), d.level) == levels.end())
        {
            levels.push_back(d.level);
        }
    }
    CHECK(levels == std::vector<std::string>{ "broken", "overridden", "syntax" }); // good has nothing to say
}
