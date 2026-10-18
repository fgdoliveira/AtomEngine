#include "Ride/Ride.h"

#include <doctest/doctest.h>

#include <glm/geometric.hpp>

#include <cmath>
#include <vector>

using namespace Demo;

namespace
{
    // A straight 100 m road north (+Z), then one long bend to the east.
    const std::vector<glm::vec3> Points = {
        { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 100.0f }, { 30.0f, 0.0f, 160.0f }, { 90.0f, 0.0f, 190.0f } };

    int FramesToFinish(RideController& ride, const RideInput& input = {})
    {
        int frames = 0;
        while (!ride.State().done && frames < 100000)
        {
            ride.Update(1.0f / 60.0f, input);
            ++frames;
        }
        return frames;
    }
}

TEST_CASE("A road is measured by length and passes through its points")
{
    const RoadPath straight({ { 0, 0, 0 }, { 0, 0, 50 } });
    CHECK(straight.Length() == doctest::Approx(50.0f));
    CHECK(straight.PointAt(25.0f).z == doctest::Approx(25.0f));
    CHECK(straight.TangentAt(10.0f).z == doctest::Approx(1.0f));
    CHECK(straight.PointAt(-5.0f).z == doctest::Approx(0.0f)); // clamped
    CHECK(straight.PointAt(500.0f).z == doctest::Approx(50.0f));

    const RoadPath road(Points);
    CHECK(road.Length() > 100.0f + 67.0f + 67.0f - 1.0f); // at least the chords
    CHECK(road.Length() < 260.0f);
    CHECK(glm::length(road.PointAt(road.Length()) - Points.back()) < 0.01f);
    CHECK(glm::length(road.PointAt(0.0f) - Points.front()) < 0.01f);
    CHECK(RoadPath({ { 1, 2, 3 } }).Length() == 0.0f); // one point: no road, no crash
}

TEST_CASE("A cinematic ride eases up to speed, keeps to the road and ends once")
{
    const RoadPath road(Points);
    RideController ride;
    ride.Start(road, { RideStyle::Cinematic, false, 14.0f });
    CHECK(ride.State().speed == 0.0f);
    CHECK(ride.State().position.z == doctest::Approx(0.0f));
    CHECK(std::abs(ride.State().heading) < 0.03f); // facing +Z, down the road (the spline's first samples bow a little)

    for (int i = 0; i < 60; ++i) ride.Update(1.0f / 60.0f);
    const float afterOne = ride.State().speed;
    CHECK(afterOne > 3.0f);
    CHECK(afterOne < 14.0f);
    for (int i = 0; i < 300; ++i) ride.Update(1.0f / 60.0f);
    CHECK(ride.State().speed == doctest::Approx(14.0f).epsilon(0.02));

    float lastProgress = ride.State().progress;
    float maxOffset = 0.0f;
    float maxLean = 0.0f;
    while (!ride.State().done)
    {
        const RideState& s = ride.Update(1.0f / 60.0f, { 1.0f, 1.0f }); // input is ignored here
        CHECK(s.progress >= lastProgress);
        lastProgress = s.progress;
        maxOffset = std::max(maxOffset, std::abs(s.offset));
        maxLean = std::max(maxLean, std::abs(s.lean));
    }
    CHECK(maxOffset <= RideController::SwayMetres + 1e-4f);
    CHECK(maxLean > 0.02f); // it banked into the bend
    CHECK(maxLean <= RideController::MaxLean);
    CHECK(ride.State().progress == doctest::Approx(1.0f));
    CHECK(glm::length(ride.State().position - Points.back()) < 0.5f);

    // Done stays done: no more movement.
    const glm::vec3 end = ride.State().position;
    ride.Update(1.0f);
    CHECK(glm::length(ride.State().position - end) < 1e-5f);
}

TEST_CASE("Leaning goes into the turn")
{
    // The bend turns east (+X); facing +Z, east is to the left (right is -X).
    const RoadPath road(Points);
    RideController ride;
    ride.Start(road, { RideStyle::Cinematic, false, 14.0f });
    ride.SetProgress(0.55f); // inside the bend, at cruising speed
    float lean = 0.0f;
    for (int i = 0; i < 40; ++i) lean = ride.Update(1.0f / 60.0f).lean;
    CHECK(lean < -0.02f); // a left turn leans left
}

TEST_CASE("A reverse ride starts at the road's end, faces back and arrives at its start")
{
    const RoadPath road(Points);
    RideController ride;
    ride.Start(road, { RideStyle::Cinematic, true, 14.0f });
    CHECK(glm::length(ride.State().position - Points.back()) < 0.01f);
    // Facing back along the last segment: west-south-west, mostly -X.
    CHECK(std::sin(ride.State().heading) < -0.5f);
    FramesToFinish(ride);
    CHECK(glm::length(ride.State().position - Points.front()) < 0.5f);
    CHECK(ride.State().heading == doctest::Approx(3.14159f).epsilon(0.01)); // facing -Z
}

TEST_CASE("Steering moves across the road on a soft leash and nudges the speed")
{
    const RoadPath road({ { 0, 0, 0 }, { 0, 0, 2000 } });
    RideController ride;
    RideSettings settings{ RideStyle::Steer, false, 14.0f, 3.0f, 2.5f };
    ride.Start(road, settings);
    for (int i = 0; i < 300; ++i) ride.Update(1.0f / 60.0f); // up to speed, straight
    CHECK(std::abs(ride.State().heading) < 1e-3f);
    for (int i = 0; i < 20; ++i) ride.Update(1.0f / 60.0f, { 1.0f, 0.0f });
    // Steering right swings the bike's heading right (toward -X: a smaller angle).
    CHECK(ride.State().heading < -0.05f);
    for (int i = 0; i < 600; ++i) ride.Update(1.0f / 60.0f, { 1.0f, 0.0f }); // hard right, 10 s
    const float offset = ride.State().offset;
    CHECK(offset > 2.5f);                      // it went right
    CHECK(offset <= 3.0f * 1.33f + 1e-4f);     // but the leash held
    CHECK(ride.State().position.x < -2.5f);    // right of +Z is -X

    for (int i = 0; i < 600; ++i) ride.Update(1.0f / 60.0f, { 0.0f, 0.0f });
    CHECK(std::abs(ride.State().offset) <= 3.0f + 0.05f); // let go: pulled back inside

    for (int i = 0; i < 600; ++i) ride.Update(1.0f / 60.0f, { 0.0f, 1.0f });
    CHECK(ride.State().speed == doctest::Approx(14.0f * RideController::SpeedRange[1]).epsilon(0.02));
    for (int i = 0; i < 600; ++i) ride.Update(1.0f / 60.0f, { 0.0f, -1.0f });
    CHECK(ride.State().speed == doctest::Approx(14.0f * RideController::SpeedRange[0]).epsilon(0.02)); // slower, never stopped
}

TEST_CASE("Jumping along the route lands there at cruising speed; frame rate doesn't change the ride")
{
    const RoadPath road(Points);
    RideController ride;
    ride.Start(road, { RideStyle::Cinematic, false, 14.0f });
    ride.SetProgress(0.98f);
    CHECK(ride.State().progress == doctest::Approx(0.98f));
    CHECK(ride.State().speed == doctest::Approx(14.0f));
    CHECK(FramesToFinish(ride) < 60 * 2); // the last 2 %: well under 2 s
    ride.SetProgress(2.0f);
    CHECK(ride.State().done);

    // The same ride at 30 and 144 fps takes the same time, give or take a frame.
    RideController slow;
    RideController fast;
    slow.Start(road, { RideStyle::Cinematic, false, 14.0f });
    fast.Start(road, { RideStyle::Cinematic, false, 14.0f });
    int slowFrames = 0;
    int fastFrames = 0;
    while (!slow.State().done) { slow.Update(1.0f / 30.0f); ++slowFrames; }
    while (!fast.State().done) { fast.Update(1.0f / 144.0f); ++fastFrames; }
    CHECK(slowFrames / 30.0f == doctest::Approx(fastFrames / 144.0f).epsilon(0.02));
}
