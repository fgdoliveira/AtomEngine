#include "Assets/Skin.h"
#include "Character/Animator.h"
#include "Character/LabViewer.h"

#include <doctest/doctest.h>

#include <glm/gtc/quaternion.hpp>

#include <cmath>
#include <map>
#include <string>

using namespace AtomFramework; // v0.0.14: the world layer

namespace
{
    // Rudy's clips: Idle 2.0 s, Jump 1.4 s, Run 0.7 s, Walk 1.1 s.
    std::optional<Animator::ClipInfo> RudyClips(std::string_view name)
    {
        static const std::map<std::string, Animator::ClipInfo, std::less<>> clips{
            { "Idle", { 0, 2.0f } }, { "Jump", { 1, 1.4f } }, { "Run", { 2, 0.7f } }, { "Walk", { 3, 1.1f } } };
        const auto found = clips.find(name);
        return found != clips.end() ? std::optional{ found->second } : std::nullopt;
    }

    AnimatorData Locomotion()
    {
        AnimatorData data;
        data.initial = "idle";
        data.states["idle"].clip = "Idle";
        AnimStateData& move = data.states["move"];
        move.blendFrom = "Walk";
        move.blendTo = "Run";
        move.param = "speed";
        move.rangeLow = 1.4f;
        move.rangeHigh = 4.0f;
        AnimStateData& jump = data.states["jump"];
        jump.clip = "Jump";
        jump.loop = false;
        jump.next = "idle";
        data.transitions = {
            { "*", "jump", { *ParseCondition("grounded == 0") }, 0.1f },
            { "idle", "move", { *ParseCondition("speed > 0.2") }, 0.25f },
            { "move", "idle", { *ParseCondition("speed <= 0.2") }, 0.3f },
        };
        return data;
    }

    float TotalWeight(const std::vector<Atom::ClipSample>& samples)
    {
        float total = 0.0f;
        for (const Atom::ClipSample& sample : samples)
        {
            total += sample.weight;
        }
        return total;
    }
}

TEST_CASE("Blending poses: the ends are the sources, rotation takes the short way")
{
    Atom::Pose a(1);
    Atom::Pose b(1);
    a[0].translation = { 0.0f, 0.0f, 0.0f };
    b[0].translation = { 2.0f, 4.0f, 0.0f };
    a[0].rotation = glm::angleAxis(glm::radians(10.0f), glm::vec3{ 0.0f, 1.0f, 0.0f });
    // The same 30-degree turn written as -q: a naive blend would swing the
    // long way round, through 190 degrees.
    b[0].rotation = -glm::angleAxis(glm::radians(30.0f), glm::vec3{ 0.0f, 1.0f, 0.0f });

    Atom::Pose out;
    Atom::BlendPoses(a, b, 0.0f, out);
    CHECK(out[0].translation.x == doctest::Approx(0.0f));
    CHECK(std::abs(glm::dot(out[0].rotation, a[0].rotation)) == doctest::Approx(1.0f));
    Atom::BlendPoses(a, b, 1.0f, out);
    CHECK(out[0].translation.y == doctest::Approx(4.0f));
    CHECK(std::abs(glm::dot(out[0].rotation, b[0].rotation)) == doctest::Approx(1.0f));

    Atom::BlendPoses(a, b, 0.5f, out);
    CHECK(out[0].translation.x == doctest::Approx(1.0f));
    const float angle = glm::degrees(2.0f * std::acos(std::min(1.0f, std::abs(out[0].rotation.w))));
    CHECK(angle == doctest::Approx(20.0f).epsilon(0.01));
}

TEST_CASE("Conditions parse from text")
{
    const auto condition = ParseCondition("speed >= 1.5");
    REQUIRE(condition);
    CHECK(condition->param == "speed");
    CHECK(condition->Holds(1.5f));
    CHECK_FALSE(condition->Holds(1.4f));
    CHECK_FALSE(ParseCondition("speed"));
    CHECK_FALSE(ParseCondition("speed ~ 1"));
    CHECK_FALSE(ParseCondition("speed > fast"));
}

TEST_CASE("Binding reports what's missing")
{
    Animator animator;
    AnimatorData data = Locomotion();
    CHECK_FALSE(animator.Bind(data, RudyClips));
    data.states["dance"].clip = "Dance";
    const auto error = animator.Bind(data, RudyClips);
    REQUIRE(error);
    CHECK(error->find("Dance") != std::string::npos);
    data = Locomotion();
    data.initial = "sleep";
    CHECK(animator.Bind(data, RudyClips));
}

TEST_CASE("The state machine follows scripted parameters")
{
    Animator animator;
    REQUIRE_FALSE(animator.Bind(Locomotion(), RudyClips));
    animator.SetParam("grounded", 1.0f);
    CHECK(animator.GetStateName() == "idle");

    animator.SetParam("speed", 3.0f);
    animator.Update(0.05f);
    CHECK(animator.GetStateName() == "move");
    CHECK(animator.IsBlending()); // crossfading out of idle
    for (int i = 0; i < 10; ++i)
    {
        animator.Update(0.05f);
    }
    CHECK_FALSE(animator.IsBlending());

    // A jump interrupts anything, plays once, and returns by itself.
    animator.SetParam("grounded", 0.0f);
    animator.Update(0.05f);
    CHECK(animator.GetStateName() == "jump");
    animator.SetParam("grounded", 1.0f);
    animator.SetParam("speed", 0.0f);
    for (int i = 0; i < 27; ++i) // 1.35 s: not over yet
    {
        animator.Update(0.05f);
    }
    CHECK(animator.GetStateName() == "jump");
    animator.Update(0.1f);
    CHECK(animator.GetStateName() == "idle");

    // Weights always add up to one, even mid-fade.
    animator.SetParam("speed", 2.0f);
    for (int i = 0; i < 8; ++i)
    {
        animator.Update(0.03f);
        CHECK(TotalWeight(animator.GetSamples()) == doctest::Approx(1.0f));
    }
}

TEST_CASE("Walk and run blended stay in phase")
{
    // Halfway between a 1.1 s walk and a 0.7 s run, the cycle lasts 0.9 s,
    // and both clips are always at the same fraction of their cycle.
    const SyncedBlend half = MakeSyncedBlend(1.1f, 0.7f, 0.5f);
    CHECK(half.duration == doctest::Approx(0.9f));

    Animator animator;
    REQUIRE_FALSE(animator.Bind(Locomotion(), RudyClips));
    animator.ForceState("move");
    animator.SetParam("grounded", 1.0f); // unset parameters read 0: airborne
    animator.SetParam("speed", 2.7f); // weight 0.5
    for (int i = 0; i < 40; ++i)
    {
        animator.Update(0.037f);
        const auto samples = animator.GetSamples();
        REQUIRE(samples.size() == 2);
        const float walkPhase = samples[0].time / 1.1f;
        const float runPhase = samples[1].time / 0.7f;
        CHECK(walkPhase == doctest::Approx(runPhase).epsilon(1e-4));
        CHECK(samples[0].weight == doctest::Approx(0.5f));
    }
    // After exactly one blended cycle, both are back where they started.
    animator.ForceState("move");
    animator.Update(0.45f);
    CHECK(animator.GetSamples()[0].time / 1.1f == doctest::Approx(0.5f));
}

TEST_CASE("The viewer crossfades between clips and runs the demo script")
{
    LabViewer viewer;
    viewer.Reset(LabViewer::Orbit{}, { { "Idle", 2.0f }, { "Jump", 1.4f }, { "Run", 0.7f }, { "Walk", 1.1f } });
    viewer.Update({}, 0.5f);
    ViewerInput pick;
    pick.selectClip = 3;
    viewer.Update(pick, 0.0f);
    auto samples = viewer.GetSamples();
    REQUIRE(samples.size() == 2); // the idle fading out, the walk in
    CHECK(samples[0].clip == 0);
    CHECK(samples[0].weight == doctest::Approx(1.0f));
    viewer.Update({}, LabViewer::CrossfadeSeconds * 0.5f);
    samples = viewer.GetSamples();
    CHECK(samples[0].weight == doctest::Approx(0.5f));
    CHECK(TotalWeight(samples) == doctest::Approx(1.0f));
    viewer.Update({}, LabViewer::CrossfadeSeconds);
    CHECK(viewer.GetSamples().size() == 1);

    ViewerInput blend;
    blend.selectMode = static_cast<int>(ViewerMode::Blend);
    blend.blendDelta = 0.25f;
    viewer.Update(blend, 0.0f);
    CHECK(viewer.GetMode() == ViewerMode::Blend);
    CHECK(viewer.GetBlendCycle() == doctest::Approx(1.0f)); // 1.1 + (0.7 - 1.1) * 0.25

    // The demo: standing, running, a jump at full speed, standing again.
    CHECK(LabViewer::Demo(1.0f).speed == 0.0f);
    CHECK(LabViewer::Demo(7.0f).speed == doctest::Approx(4.5f));
    CHECK_FALSE(LabViewer::Demo(6.9f).grounded);
    CHECK(LabViewer::Demo(13.0f).speed == 0.0f);
}
