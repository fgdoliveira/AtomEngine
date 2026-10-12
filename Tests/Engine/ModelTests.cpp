#include "Assets/Model.h"

#include <doctest/doctest.h>

using namespace Atom;

// What shipped models carry: the apps' content tests and
// Tests/Content/KitModelTests.cpp.
TEST_CASE("A missing model file loads nothing")
{
    const char* const missing = "no/such/model.glb";
    CHECK(LoadModelGeometry(missing).empty());
    CHECK(LoadModelMaterials(missing).empty());
    CHECK(LoadModelAnimations(missing).empty());
}
