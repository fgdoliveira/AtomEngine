// DRIFT's world and flow rules (M78), pinned to the three.js original
// (3d-game-00/src/world.js, src/interact.js).
#include "Flight.h"
#include "SpeedField.h"
#include "World.h"

#include <doctest/doctest.h>

#include <algorithm>

using namespace Drift;
using doctest::Approx;

namespace
{
    int Count(const World& world, Kind kind)
    {
        return static_cast<int>(std::count_if(world.Things().begin(), world.Things().end(),
                                              [&](const Thing& t) { return t.kind == kind; }));
    }
}

TEST_CASE("Drift world: segments every 80 m, 720 m ahead; a ring and 7 orbs each")
{
    World world(1);
    world.Update(0.016f, 0.0f, 0.0f);
    // From z = -60 down past -720: segments at -60, -140, ... -700.
    CHECK(world.Segments() == 9);
    CHECK(Count(world, Kind::Ring) == 9);
    CHECK(Count(world, Kind::Orb) == 9 * 7);
    for (const Thing& t : world.Things())
    {
        if (t.kind == Kind::Ring)
        {
            const glm::vec2 c = Path(t.position.z);
            CHECK(t.position.x == Approx(c.x)); // on the path
            CHECK(t.position.y == Approx(c.y));
        }
    }
}

TEST_CASE("Drift world: 2 + min(4, segment / 4) rocks per segment, sized 0.8..2.6")
{
    World world(3);
    world.Update(0.016f, 0.0f, 0.0f); // segments 0..8: 2,2,2,2,3,3,3,3,4
    CHECK(Count(world, Kind::Rock) == 2 * 4 + 3 * 4 + 4);
    for (const Thing& t : world.Things())
    {
        if (t.kind == Kind::Rock)
        {
            CHECK(t.scale >= 0.8f);
            CHECK(t.scale <= 2.6f);
            CHECK(t.radius == Approx(t.scale * 1.05f));
        }
    }
}

TEST_CASE("Drift world: what's 20 m behind is recycled; the same seed gives the same course")
{
    World a(42);
    World b(42);
    a.Update(0.016f, 0.0f, 0.0f);
    b.Update(0.016f, 0.0f, 0.0f);
    REQUIRE(a.Things().size() == b.Things().size());
    CHECK(a.Things()[5].position.x == b.Things()[5].position.x);

    a.Update(0.016f, 0.0f, -300.0f);
    for (const Thing& t : a.Things())
    {
        CHECK(t.position.z <= -300.0f + Behind);
    }
}

TEST_CASE("Drift flow: a ring passed within 3.1 m gives +0.10, a miss -0.08")
{
    World world(1);
    world.Update(0.0f, 0.0f, 0.0f);
    Flow flow;
    flow.value = 0.5f;
    const Thing ring = world.Things().front(); // the first ring, at z = -60
    // Cross its plane through its centre.
    FlowEvents e = flow.Check(world, { ring.position.x, ring.position.y, ring.position.z - 1.0f }, ring.position.z + 1.0f, 0.0f);
    CHECK(e.ringsPassed == 1);
    CHECK(flow.value == Approx(0.6f));

    World far(1);
    far.Update(0.0f, 0.0f, 0.0f);
    Flow miss;
    miss.value = 0.5f;
    e = miss.Check(far, { ring.position.x + 5.0f, ring.position.y, ring.position.z - 1.0f }, ring.position.z + 1.0f, 0.0f);
    CHECK(e.ringsMissed == 1);
    CHECK(miss.value == Approx(0.42f));
}

TEST_CASE("Drift flow: an orb gives +0.025 and chain +1; a rock -0.25 and chain 0")
{
    World world(1);
    world.Update(0.0f, 0.0f, 0.0f);
    Flow flow;
    flow.value = 0.5f;
    flow.chain = 4;
    auto orb = std::find_if(world.Things().begin(), world.Things().end(), [](const Thing& t) { return t.kind == Kind::Orb; });
    FlowEvents e = flow.Check(world, orb->position, orb->position.z + 0.5f, 0.0f);
    CHECK(e.orbs >= 1);
    CHECK(flow.chain == 4 + e.orbs);
    CHECK(flow.value == Approx(0.5f + 0.025f * static_cast<float>(e.orbs)));

    auto rock = std::find_if(world.Things().begin(), world.Things().end(), [](const Thing& t) { return t.kind == Kind::Rock; });
    const float before = flow.value;
    e = flow.Check(world, rock->position, rock->position.z + 0.5f, 0.0f);
    CHECK(e.rockHit);
    CHECK(flow.chain == 0);
    CHECK(flow.value <= before - 0.25f + 0.1f + 1e-4f); // -0.25, plus anything else crossed
}

TEST_CASE("Drift flow: decays 0.012 per second and stays within 0..1")
{
    World world(1);
    Flow flow;
    flow.value = 0.5f;
    flow.Check(world, { 0.0f, 0.0f, 1000.0f }, 1000.0f, 10.0f);
    CHECK(flow.value == Approx(0.5f - 0.12f));
    flow.Check(world, { 0.0f, 0.0f, 1000.0f }, 1000.0f, 100.0f);
    CHECK(flow.value == 0.0f);
}

TEST_CASE("Drift speed field: stars wrap within 400 m, streaks within 130 m")
{
    SpeedField field(5);
    REQUIRE(field.Stars().size() == SpeedField::StarCount);
    REQUIRE(field.Streaks().size() == SpeedField::StreakCount);
    for (int i = 0; i < 300; ++i)
    {
        field.Update(1.0f / 60.0f, 120.0f);
    }
    for (const glm::vec3& s : field.Stars())
    {
        CHECK(s.z <= 0.0f);
        CHECK(s.z >= -400.0f);
    }
    for (const glm::vec3& s : field.Streaks())
    {
        CHECK(s.z <= 10.0f);
        CHECK(s.z >= -120.0f);
    }
}

TEST_CASE("Drift speed field: streaks show above 45 m/s, at most 0.55, length 0.08 s")
{
    CHECK(SpeedField::StreakOpacity(40.0f) == 0.0f);
    CHECK(SpeedField::StreakOpacity(75.0f) == Approx(0.5f));
    CHECK(SpeedField::StreakOpacity(200.0f) == Approx(0.55f));
    CHECK(SpeedField::StreakLength(100.0f) == Approx(8.0f));
}
