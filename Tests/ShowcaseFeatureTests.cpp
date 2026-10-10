// The Showcase's feature catalog (v0.0.14): what the lens claims must be
// true of the repository - a source file that exists, a manual section that
// is a real heading.
#include "Features/Features.h"

#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <set>
#include <string>

using namespace Showcase;

TEST_CASE("Every Showcase feature names a real source file and a real manual section")
{
    std::ifstream manualFile(ATOM_SOURCE_DIR "/docs/AtomEngine-Tech-Manual.md");
    REQUIRE(manualFile);
    std::set<int> sections;
    for (std::string line; std::getline(manualFile, line);)
    {
        if (line.rfind("## ", 0) == 0)
        {
            sections.insert(std::atoi(line.c_str() + 3)); // "## 65. Stylized water"
        }
    }

    std::set<std::string> ids;
    for (const FeatureInfo& feature : FeatureCatalog())
    {
        INFO(feature.id);
        CHECK(ids.insert(feature.id).second); // unique
        CHECK(std::string(feature.title).size() > 3);
        CHECK(std::string(feature.system).size() > 10);
        CHECK(std::filesystem::exists(std::string(ATOM_SOURCE_DIR "/") + feature.source));
        CHECK(sections.count(feature.manual) == 1);
    }
    CHECK(FeatureCatalog().size() <= 10); // the lens's keys: 1-9, 0
}

TEST_CASE("Every Showcase feature has its entry in docs/Features.md (ADR-009)")
{
    // A section headed "## <title> (`<id>`)" and a row in the table.
    std::ifstream file(ATOM_SOURCE_DIR "/docs/Features.md");
    REQUIRE(file);
    const std::string text{ std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>() };
    for (const FeatureInfo& feature : FeatureCatalog())
    {
        INFO(feature.id);
        CHECK(text.find("## " + std::string(feature.title) + " (`" + feature.id + "`)") != std::string::npos);
        CHECK(text.find("| " + std::string(feature.title) + " | `" + feature.id + "` |") != std::string::npos);
    }
}
