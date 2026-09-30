#include "Pachinko/Physics2D.h"

#include <doctest/doctest.h>

#include <glm/geometric.hpp>

#include <cstring>

using namespace AtomGame;

namespace
{
    PhysicsSettings NoGravity()
    {
        PhysicsSettings settings;
        settings.gravity = { 0.0f, 0.0f };
        return settings;
    }
}

TEST_CASE("Physics2D: a ball dropped on a flat rail comes to rest on it")
{
    World2D world;
    world.AddSegment({ { 0.0f, 100.0f }, { 200.0f, 100.0f } });
    world.AddBall({ 100.0f, 70.0f }, { 0.0f, 0.0f });
    for (int i = 0; i < 180; ++i) // 3 s
    {
        world.Step(1.0f / 60.0f);
    }
    const Ball& ball = world.GetBalls().front();
    CHECK(ball.position.y == doctest::Approx(100.0f - world.GetSettings().ballRadius).epsilon(0.01));
    CHECK(glm::length(ball.velocity) < 5.0f);
    CHECK(ball.position.x == doctest::Approx(100.0f)); // no drift on a flat rail
}

TEST_CASE("Physics2D: a bounce keeps the restitution's share of the speed")
{
    World2D world(NoGravity());
    world.AddSegment({ { 0.0f, 100.0f }, { 200.0f, 100.0f } });
    world.AddBall({ 100.0f, 90.0f }, { 0.0f, 200.0f });
    for (int i = 0; i < 12; ++i)
    {
        world.Step(1.0f / 60.0f);
    }
    const Ball& ball = world.GetBalls().front();
    CHECK(ball.velocity.y == doctest::Approx(-200.0f * world.GetSettings().wallRestitution).epsilon(0.02));
    CHECK(ball.position.y < 100.0f); // it came back off the rail, not through it
}

TEST_CASE("Physics2D: no tunnelling through a nail or a thin wall at top speed")
{
    const float speed = PhysicsSettings{}.maxSpeed;

    World2D nails(NoGravity());
    nails.AddNail({ { 100.0f, 50.0f }, 1.0f });
    nails.AddBall({ 20.0f, 50.0f }, { speed, 0.0f });
    for (int i = 0; i < 30; ++i)
    {
        nails.Step(1.0f / 60.0f);
    }
    CHECK(nails.GetBalls().front().position.x < 100.0f);      // bounced back
    CHECK(nails.GetBalls().front().velocity.x < 0.0f);

    World2D wall(NoGravity());
    wall.AddSegment({ { 100.0f, 0.0f }, { 100.0f, 100.0f } });
    wall.AddBall({ 20.0f, 50.0f }, { speed, 0.0f });
    for (int i = 0; i < 30; ++i)
    {
        wall.Step(1.0f / 60.0f);
    }
    CHECK(wall.GetBalls().front().position.x < 100.0f);
}

TEST_CASE("Physics2D: the same run twice gives the same bits")
{
    const auto run = [] {
        World2D world;
        for (int row = 0; row < 8; ++row)
        {
            for (int column = 0; column < 12; ++column)
            {
                world.AddNail({ { 20.0f + column * 14.0f + (row % 2) * 7.0f, 40.0f + row * 14.0f }, 1.0f });
            }
        }
        world.AddSegment({ { 10.0f, 0.0f }, { 10.0f, 200.0f } });
        world.AddSegment({ { 190.0f, 0.0f }, { 190.0f, 200.0f } });
        world.AddSegment({ { 10.0f, 200.0f }, { 190.0f, 200.0f } });
        for (int i = 0; i < 30; ++i)
        {
            world.AddBall({ 30.0f + i * 4.7f, 10.0f }, { (i % 3 - 1) * 30.0f, 0.0f });
        }
        for (int i = 0; i < 600; ++i)
        {
            world.Step(1.0f / 60.0f);
        }
        return world.GetBalls();
    };
    const std::vector<Ball> a = run();
    const std::vector<Ball> b = run();
    REQUIRE(a.size() == b.size());
    for (std::size_t i = 0; i < a.size(); ++i)
    {
        CHECK(std::memcmp(&a[i], &b[i], sizeof(Ball)) == 0);
    }
}

TEST_CASE("Physics2D: balls collide as equal masses, keeping their momentum")
{
    World2D world(NoGravity());
    world.AddBall({ 50.0f, 50.0f }, { 120.0f, 0.0f });
    world.AddBall({ 90.0f, 50.0f }, { -40.0f, 0.0f });
    world.Step(0.5f);
    const auto& balls = world.GetBalls();
    const glm::vec2 total = balls[0].velocity + balls[1].velocity;
    CHECK(total.x == doctest::Approx(80.0f).epsilon(0.001));
    CHECK(balls[0].velocity.x < balls[1].velocity.x); // they separated
    CHECK_FALSE(world.GetImpacts().empty());
}

TEST_CASE("Physics2D: closest point on a segment")
{
    CHECK(ClosestOnSegment({ 5, 5 }, { 0, 0 }, { 10, 0 }) == glm::vec2{ 5, 0 });
    CHECK(ClosestOnSegment({ -5, 5 }, { 0, 0 }, { 10, 0 }) == glm::vec2{ 0, 0 });
    CHECK(ClosestOnSegment({ 15, -5 }, { 0, 0 }, { 10, 0 }) == glm::vec2{ 10, 0 });
}
