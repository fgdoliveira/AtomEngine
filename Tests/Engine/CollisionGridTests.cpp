#include "Physics/CollisionWorld.h"

#include <doctest/doctest.h>

#include <random>

using Atom::CollisionWorld;

namespace
{
    // A synthetic world (M105): floor tiles at several heights, ramps and
    // walls scattered over 90x90 m, from a seed - the shapes a level's
    // collision has, without depending on any app's content.
    void BuildTestWorld(CollisionWorld& world)
    {
        std::mt19937 random(42);
        std::uniform_real_distribution<float> across(-45.0f, 45.0f);
        std::uniform_real_distribution<float> size(0.5f, 6.0f);
        std::uniform_real_distribution<float> height(0.0f, 3.0f);
        for (int i = 0; i < 400; ++i)
        {
            const glm::vec3 at{ across(random), height(random), across(random) };
            const float w = size(random);
            const float d = size(random);
            switch (i % 3)
            {
            case 0: // a floor tile, facing up
                world.AddTriangle(at, at + glm::vec3{ 0, 0, d }, at + glm::vec3{ w, 0, 0 });
                world.AddTriangle(at + glm::vec3{ w, 0, 0 }, at + glm::vec3{ 0, 0, d }, at + glm::vec3{ w, 0, d });
                break;
            case 1: // a ramp
                world.AddTriangle(at, at + glm::vec3{ 0, 0.5f * w, d }, at + glm::vec3{ w, 0, 0 });
                break;
            default: // a wall
                world.AddTriangle(at, at + glm::vec3{ w, 0, 0 }, at + glm::vec3{ w, 2.5f, 0 });
                world.AddTriangle(at, at + glm::vec3{ w, 2.5f, 0 }, at + glm::vec3{ 0, 2.5f, 0 });
                break;
            }
        }
    }
}

TEST_CASE("The collision grid answers exactly like a scan of every triangle")
{
    CollisionWorld grid;
    CollisionWorld scan;
    BuildTestWorld(grid);
    BuildTestWorld(scan);
    scan.SetBroadPhase(false);

    std::mt19937 random(1234);
    std::uniform_real_distribution<float> across(-45.0f, 45.0f);
    std::uniform_real_distribution<float> height(0.0f, 4.0f);
    std::uniform_real_distribution<float> step(-6.0f, 6.0f);
    int floors = 0;
    int hits = 0;
    for (int i = 0; i < 3000; ++i)
    {
        const glm::vec3 point{ across(random), height(random), across(random) };

        const auto floorGrid = grid.FindFloor(point, 6.0f);
        const auto floorScan = scan.FindFloor(point, 6.0f);
        REQUIRE(floorGrid.has_value() == floorScan.has_value());
        if (floorGrid)
        {
            REQUIRE(*floorGrid == *floorScan);
            ++floors;
        }

        const glm::vec3 to = point + glm::vec3{ step(random), step(random) * 0.3f, step(random) };
        const auto hitGrid = grid.Raycast(point, to);
        const auto hitScan = scan.Raycast(point, to);
        REQUIRE(hitGrid.has_value() == hitScan.has_value());
        if (hitGrid)
        {
            REQUIRE(hitGrid->distance == hitScan->distance);
            ++hits;
        }

        glm::vec3 a = point;
        glm::vec3 b = point;
        REQUIRE(grid.ResolveSphereHorizontal(a, 0.3f) == scan.ResolveSphereHorizontal(b, 0.3f));
        REQUIRE(a == b);
    }
    // The comparison means something only if both answers occur.
    CHECK(floors > 100);
    CHECK(hits > 100);
}

TEST_CASE("Adding collision keeps what was there")
{
    CollisionWorld world;
    world.AddTriangle({ 0, 0, 0 }, { 0, 0, 1 }, { 1, 0, 0 }); // facing up
    CHECK(world.FindFloor({ 0.2f, 1.0f, 0.2f }, 2.0f).has_value()); // the grid is built now
    BuildTestWorld(world);
    CHECK(world.GetTriangleCount() > 1);
    // The first floor still answers (grid rebuilt after the additions).
    CHECK(world.FindFloor({ 0.2f, 1.0f, 0.2f }, 2.0f).has_value());
}
