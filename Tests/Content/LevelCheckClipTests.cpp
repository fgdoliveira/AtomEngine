#include "Level/LevelCheck.h"
#include "TestAssets.h"

#include <doctest/doctest.h>

#include <string>
#include <vector>

using namespace AtomFramework;

// M112: a clip is only known once its model is read - the check opens the
// kit's real models (a fixture level over Content/).
TEST_CASE("An animation clip the model doesn't have is an error that lists the clips it has")
{
    LevelCheckOptions options;
    options.assets = AssetRoots({ ATOM_SOURCE_DIR "/Tests/Content/Fixtures/", AtomTests::SharedContent });
    const std::vector<LevelDiagnostic> found = CheckLevelFile("clips", options);
    REQUIRE(found.size() == 2);

    CHECK(found[0].entity == "mill2");
    CHECK(found[0].property == "/entities/1/animation/clip");
    CHECK(found[0].problem == "clip \"spinning\" is not in Kit/windmill.glb");
    CHECK(found[0].fix == "its clips: spin");

    CHECK(found[1].entity == "shed");
    CHECK(found[1].property == "/entities/2/interactable/action");
    CHECK(found[1].problem == "clip \"slam\" is not in Kit/shed.glb");

    options.checkClips = false; // the models aren't opened
    CHECK(CheckLevelFile("clips", options).empty());
}
