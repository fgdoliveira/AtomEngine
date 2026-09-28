#include "Physics/CollisionWorld.h"

#include <doctest/doctest.h>

#include <chrono>
#include <filesystem>
#include <random>
#include <string>

using namespace Atom;

namespace
{
    const std::string Assets = ATOM_SOURCE_DIR "/Assets/";
}

TEST_CASE("The collision grid answers exactly like a scan of every triangle")
{
    int files = 0;
    for (const char* folder : { "Street", "Shrine", "Interior", "Fields" })
    {
        for (const auto& entry : std::filesystem::directory_iterator(Assets + folder))
        {
            const std::string name = entry.path().filename().string();
            if (name.find("_col") == std::string::npos || entry.path().extension() != ".glb")
            {
                continue;
            }
            ++files;
            INFO(name);
            CollisionWorld grid;
            CollisionWorld scan;
            REQUIRE(grid.Load(entry.path().string()));
            REQUIRE(scan.Load(entry.path().string()));
            scan.SetBroadPhase(false);

            std::mt19937 random(1234);
            std::uniform_real_distribution<float> across(-45.0f, 45.0f);
            std::uniform_real_distribution<float> height(0.0f, 4.0f);
            std::uniform_real_distribution<float> step(-6.0f, 6.0f);
            for (int i = 0; i < 3000; ++i)
            {
                const glm::vec3 point{ across(random), height(random), across(random) };

                const auto floorGrid = grid.FindFloor(point, 6.0f);
                const auto floorScan = scan.FindFloor(point, 6.0f);
                REQUIRE(floorGrid.has_value() == floorScan.has_value());
                if (floorGrid)
                {
                    REQUIRE(*floorGrid == *floorScan);
                }

                const glm::vec3 to = point + glm::vec3{ step(random), step(random) * 0.3f, step(random) };
                const auto hitGrid = grid.Raycast(point, to);
                const auto hitScan = scan.Raycast(point, to);
                REQUIRE(hitGrid.has_value() == hitScan.has_value());
                if (hitGrid)
                {
                    REQUIRE(hitGrid->distance == hitScan->distance);
                }

                glm::vec3 a = point;
                glm::vec3 b = point;
                REQUIRE(grid.ResolveSphereHorizontal(a, 0.3f) == scan.ResolveSphereHorizontal(b, 0.3f));
                REQUIRE(a == b);
            }
        }
    }
    CHECK(files >= 4);
}

TEST_CASE("Appending collision keeps what was there")
{
    CollisionWorld world;
    world.AddTriangle({ 0, 0, 0 }, { 0, 0, 1 }, { 1, 0, 0 }); // facing up
    REQUIRE(world.Append(Assets + "Interior/interior_col.glb"));
    CHECK(world.GetTriangleCount() > 1);
    // The hand-added floor still answers (grid rebuilt after the append).
    CHECK(world.FindFloor({ 0.2f, 1.0f, 0.2f }, 2.0f).has_value());
}

TEST_CASE("The grid makes the street's queries much cheaper")
{
    CollisionWorld grid;
    CollisionWorld scan;
    REQUIRE(grid.Load(Assets + "Street/street_col.glb"));
    REQUIRE(scan.Load(Assets + "Street/street_col.glb"));
    scan.SetBroadPhase(false);

    const auto time = [](CollisionWorld& world) {
        const auto start = std::chrono::steady_clock::now();
        float sink = 0.0f;
        for (int i = 0; i < 2000; ++i)
        {
            glm::vec3 center{ -40.0f + 0.04f * i, 1.0f, 0.5f };
            world.ResolveSphereHorizontal(center, 0.3f);
            sink += world.FindFloor(center, 3.0f).value_or(0.0f);
        }
        const auto elapsed = std::chrono::steady_clock::now() - start;
        return std::chrono::duration<double>(elapsed).count() + sink * 0.0;
    };
    grid.FindFloor({ 0, 1, 0 }, 1.0f); // build the grid outside the timing
    const double withGrid = time(grid);
    const double withScan = time(scan);
    MESSAGE("street: grid " << withGrid * 1000.0 << " ms, scan " << withScan * 1000.0 << " ms for 2000 steps");
    CHECK(withGrid < withScan);
}
