#include "Character/LabViewer.h"

#include <doctest/doctest.h>

#include <glm/geometric.hpp>
#include <glm/trigonometric.hpp>

#include <cmath>

using namespace AtomGame;

namespace
{
    LabViewer MakeViewer()
    {
        LabViewer viewer;
        viewer.Reset(LabViewer::Orbit{}, { { "Idle", 2.0f }, { "Jump", 1.4f }, { "Run", 0.7f }, { "Walk", 1.1f } });
        return viewer;
    }

    // Atom::Camera's forward for a yaw/pitch (radians).
    glm::vec3 Forward(float yaw, float pitch)
    {
        return { std::sin(yaw) * std::cos(pitch), std::sin(pitch), -std::cos(yaw) * std::cos(pitch) };
    }
}

TEST_CASE("The orbit camera always looks at the target")
{
    LabViewer viewer = MakeViewer();
    for (const float yaw : { 0.0f, 45.0f, 170.0f, -120.0f })
    {
        for (const float pitch : { 0.0f, 30.0f, -5.0f })
        {
            ViewerInput in;
            in.orbitYawDegrees = yaw;
            in.orbitPitchDegrees = pitch;
            viewer.Update(in, 0.0f);
            const glm::vec3 toTarget = glm::normalize(viewer.GetOrbit().target - viewer.GetEye());
            const glm::vec3 forward = Forward(viewer.GetCameraYaw(), viewer.GetCameraPitch());
            CHECK(glm::dot(toTarget, forward) == doctest::Approx(1.0f).epsilon(1e-4));
            CHECK(glm::length(viewer.GetEye() - viewer.GetOrbit().target)
                  == doctest::Approx(viewer.GetOrbit().distance).epsilon(1e-4));
        }
    }
}

TEST_CASE("The orbit stays within its limits")
{
    LabViewer viewer = MakeViewer();
    ViewerInput in;
    in.orbitPitchDegrees = 500.0f;
    in.zoomSteps = 100.0f;
    viewer.Update(in, 0.0f);
    CHECK(viewer.GetOrbit().pitchDegrees == LabViewer::MaxPitch);
    CHECK(viewer.GetOrbit().distance == LabViewer::MinDistance);

    in.orbitPitchDegrees = -500.0f;
    in.zoomSteps = -100.0f;
    viewer.Update(in, 0.0f);
    CHECK(viewer.GetOrbit().pitchDegrees == LabViewer::MinPitch);
    CHECK(viewer.GetOrbit().distance == LabViewer::MaxDistance);

    // Yaw wraps instead: always within one turn.
    in = {};
    in.orbitYawDegrees = 1000.0f;
    viewer.Update(in, 0.0f);
    CHECK(std::abs(viewer.GetOrbit().yawDegrees) <= 180.0f);
}

TEST_CASE("Clips play, loop, pause, step and change speed")
{
    LabViewer viewer = MakeViewer();
    CHECK(viewer.GetClipName() == "Idle");

    ViewerInput pick;
    pick.selectClip = 2;
    viewer.Update(pick, 0.0f);
    CHECK(viewer.GetClipName() == "Run");
    CHECK(viewer.GetTime() == 0.0f);

    // Every clip loops in the viewer: 0.9 s into a 0.7 s run is 0.2 s.
    viewer.Update({}, 0.9f);
    CHECK(viewer.GetTime() == doctest::Approx(0.2f));

    ViewerInput pause;
    pause.togglePause = true;
    viewer.Update(pause, 0.0f);
    viewer.Update({}, 0.3f);
    CHECK(viewer.IsPaused());
    CHECK(viewer.GetTime() == doctest::Approx(0.2f));

    ViewerInput step;
    step.step = true;
    viewer.Update(step, 0.3f); // one frame, whatever the frame time
    CHECK(viewer.GetTime() == doctest::Approx(0.2f + LabViewer::StepSeconds));
    CHECK(viewer.IsPaused());

    viewer.Update(pause, 0.0f); // play again, at half speed
    ViewerInput slower;
    slower.speedStep = -1;
    viewer.Update(slower, 0.0f);
    CHECK(viewer.GetSpeed() == 0.5f);
    const float before = viewer.GetTime();
    viewer.Update({}, 0.2f);
    CHECK(viewer.GetTime() == doctest::Approx(before + 0.1f));

    // Speed stops at its ends.
    for (int i = 0; i < 20; ++i)
    {
        viewer.Update(slower, 0.0f);
    }
    CHECK(viewer.GetSpeed() == LabViewer::Speeds.front());
}

TEST_CASE("Toggles and the bind pose")
{
    LabViewer viewer = MakeViewer();
    ViewerInput in;
    in.toggleBindPose = true;
    in.toggleSkeleton = true;
    in.toggleWeights = true;
    viewer.Update(in, 0.0f);
    CHECK(viewer.ShowsBindPose());
    CHECK(viewer.ShowsSkeleton());
    CHECK(viewer.ShowsWeights());
    CHECK(viewer.GetPoseClip() == -1); // the bind pose draws no clip

    // Picking a clip means "show me it": the bind pose goes.
    CHECK(viewer.SelectClip("Walk"));
    CHECK_FALSE(viewer.ShowsBindPose());
    CHECK(viewer.GetPoseClip() == 3);
    CHECK_FALSE(viewer.SelectClip("Dance"));
    CHECK_FALSE(viewer.SelectClip(7));
}
