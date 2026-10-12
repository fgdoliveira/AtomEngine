#include "Physics/CollisionWorld.h"

#include <doctest/doctest.h>

using Atom::CollisionWorld;

namespace
{
    // A 2x2 m wall in the plane z = 0, facing +Z.
    CollisionWorld MakeWall()
    {
        CollisionWorld world;
        world.AddTriangle({ -1, 0, 0 }, { 1, 0, 0 }, { 1, 2, 0 });
        world.AddTriangle({ -1, 0, 0 }, { 1, 2, 0 }, { -1, 2, 0 });
        return world;
    }
}

TEST_CASE("A segment through the wall hits it at the right distance")
{
    const CollisionWorld wall = MakeWall();
    const auto hit = wall.Raycast({ 0, 1, 3 }, { 0, 1, -3 });

    REQUIRE(hit.has_value());
    CHECK(hit->distance == doctest::Approx(3.0f));
    CHECK(hit->point.z == doctest::Approx(0.0f));
}

TEST_CASE("Hits count from the back side too")
{
    const CollisionWorld wall = MakeWall();
    CHECK(wall.Raycast({ 0, 1, -3 }, { 0, 1, 3 }).has_value());
}

TEST_CASE("Segments that stop short, pass beside or run parallel miss")
{
    const CollisionWorld wall = MakeWall();
    CHECK_FALSE(wall.Raycast({ 0, 1, 3 }, { 0, 1, 0.5f }).has_value());  // stops short
    CHECK_FALSE(wall.Raycast({ 3, 1, 3 }, { 3, 1, -3 }).has_value());    // beside
    CHECK_FALSE(wall.Raycast({ -3, 1, 1 }, { 3, 1, 1 }).has_value());    // parallel
}

TEST_CASE("The nearest of several hits is reported")
{
    CollisionWorld world = MakeWall();
    // A second wall at z = -2.
    world.AddTriangle({ -1, 0, -2 }, { 1, 0, -2 }, { 1, 2, -2 });
    world.AddTriangle({ -1, 0, -2 }, { 1, 2, -2 }, { -1, 2, -2 });

    const auto hit = world.Raycast({ 0, 1, 3 }, { 0, 1, -5 });
    REQUIRE(hit.has_value());
    CHECK(hit->distance == doctest::Approx(3.0f));
}

TEST_CASE("Degenerate triangles are ignored")
{
    CollisionWorld world;
    world.AddTriangle({ 0, 0, 0 }, { 1, 0, 0 }, { 2, 0, 0 });
    CHECK(world.GetTriangleCount() == 0);
}
