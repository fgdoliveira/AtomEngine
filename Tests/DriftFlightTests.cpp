// DRIFT's flight (M77) pinned to the three.js original's formulas
// (3d-game-00/src/world.js, src/ship.js).
#include "Flight.h"

#include <doctest/doctest.h>

#include <cmath>

using namespace Drift;
using doctest::Approx;

TEST_CASE("Drift path: the original's two sines and a cosine")
{
    for (float z : { 0.0f, -100.0f, -523.0f, -2000.0f })
    {
        const glm::vec2 c = Path(z);
        CHECK(c.x == Approx(std::sin(z * 0.012f) * 14.0f + std::sin(z * 0.031f) * 4.0f));
        CHECK(c.y == Approx(std::cos(z * 0.009f) * 6.0f));
    }
}

TEST_CASE("Drift ship: cruise speed eases toward 38 + flow * 34")
{
    Ship ship;
    for (int i = 0; i < 2000; ++i) // ~33 s at 60 fps: settled
    {
        ship.Update(1.0f / 60.0f, 0.5f, {});
    }
    CHECK(ship.speed == Approx(38.0f + 0.5f * 34.0f).epsilon(0.001));
    CHECK(ship.position.z < -1000.0f); // flies toward -Z
}

TEST_CASE("Drift ship: boost multiplies forward speed by up to 1.7 and stretches the model")
{
    Ship ship;
    float forward = 0.0f;
    for (int i = 0; i < 600; ++i)
    {
        forward = ship.Update(1.0f / 60.0f, 0.0f, { 0.0f, 0.0f, true });
    }
    CHECK(ship.boost == Approx(1.0f).epsilon(0.001));
    CHECK(forward == Approx(ship.speed * 1.7f).epsilon(0.001));
    CHECK(ship.modelScale.z == Approx(1.22f).epsilon(0.001));
    CHECK(ship.modelScale.x == Approx(0.9f).epsilon(0.001));
}

TEST_CASE("Drift ship: the soft leash keeps it within 11 m of the path")
{
    Ship ship;
    for (int i = 0; i < 1200; ++i)
    {
        ship.Update(1.0f / 60.0f, 0.0f, { 1.0f, 1.0f, false }); // hold up-right
        const glm::vec2 c = Path(ship.position.z);
        CHECK(std::hypot(ship.position.x - c.x, ship.position.y - c.y) <= Bound + 1e-3f);
    }
}

TEST_CASE("Drift ship: lateral velocity eases toward input * (16, 12)")
{
    Ship ship;
    for (int i = 0; i < 600; ++i)
    {
        ship.Update(1.0f / 60.0f, 0.0f, { 0.5f, -0.5f, false });
    }
    CHECK(ship.velocity.x == Approx(8.0f).epsilon(0.001));
    CHECK(ship.velocity.y == Approx(-6.0f).epsilon(0.001));
    CHECK(ship.modelRotation.z == Approx(-8.0f * 0.05f).epsilon(0.001)); // banks into the turn
}

TEST_CASE("Drift camera: FOV eases toward 70 + boost * 18 + flow * 6; looks 12 m ahead")
{
    Ship ship;
    for (int i = 0; i < 600; ++i)
    {
        ship.Update(1.0f / 60.0f, 1.0f, { 0.0f, 0.0f, true });
    }
    CHECK(ship.Camera().fovDegrees == Approx(70.0f + 18.0f + 6.0f).epsilon(0.001));
    CHECK(ship.Camera().target.z == Approx(ship.position.z - 12.0f));
    CHECK(ship.Camera().position.z > ship.position.z); // behind the ship
}

TEST_CASE("Drift camera: shake decays at 2 per second and is reproducible")
{
    Ship a(7);
    Ship b(7);
    a.shake = b.shake = 1.0f;
    a.Update(0.1f, 0.0f, {});
    b.Update(0.1f, 0.0f, {});
    CHECK(a.shake == Approx(0.8f));
    CHECK(a.Camera().position.x == Approx(b.Camera().position.x)); // same seed, same shake
}
