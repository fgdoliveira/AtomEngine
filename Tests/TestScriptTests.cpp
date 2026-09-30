#include "Testing/TestScript.h"

#include <doctest/doctest.h>

#include <set>

using namespace AtomGame;

namespace
{
    // A scripted stand-in for the game: just enough state to check that the
    // runner sequences, waits and fails correctly.
    struct FakeGame : TestHooks
    {
        std::set<std::string> flags;
        std::string level = "street";
        std::string mode = "exploring";
        std::string target = "keeper";
        std::string message;
        int interactions = 0;
        std::size_t voices = 3;
        float timeUntilLevelChange = -1.0f; // simulated transition
        std::string surface = "dirt";
        ArrivalError arrivalAfterChange{};   // how the next level is entered
        ArrivalError arrival{};

        bool TeleportTo(const std::string& entity, float) override { return entity != "missing"; }
        void Teleport(const glm::vec3& to, float yaw) override { feet = to; yawDegrees = yaw; }
        float yawDegrees = 0.0f;
        bool Face(const std::string& entity) override { return entity != "missing"; }
        std::string CurrentTarget() override { return target; }
        bool Interact() override
        {
            ++interactions;
            message = "You bow.";
            flags.insert("bowed");
            return !target.empty();
        }
        bool Choose(int index) override { return index >= 0 && index < 2; }
        void Advance() override { mode = "exploring"; }
        bool HasFlag(const std::string& flag) const override { return flags.contains(flag); }
        std::string LevelName() const override { return level; }
        std::string ModeName() const override { return mode; }
        std::string Message() const override { return message; }
        std::string DialogueNodeId() const override { return ""; }
        std::size_t VoiceCount() const override { return voices; }
        std::string SurfaceName() const override { return surface; }
        float ZoneLevel(const std::string&) const override { return 0.0f; }
        std::pair<std::uint32_t, std::uint32_t> ScreenStats() const override { return { 0, 0 }; }
        std::string Capture(const std::string& stem, bool) override { captures.push_back(stem); return {}; }
        bool CapturePending() const override { return false; }
        bool Set(const std::string& what, const std::string& value) override
        {
            settings.push_back(what + "=" + value);
            return what != "nonsense";
        }
        std::vector<std::string> captures;
        std::vector<std::string> settings;
        bool HoldAction(const std::string& action, bool) override { return action != "nonsense"; }
        bool PressAction(const std::string& action) override { return action != "nonsense"; }
        ArrivalError Arrival() const override { return arrival; }
        std::optional<float> animationTime;
        bool animationPlaying = false;
        std::optional<float> AnimationTime(const std::string&) const override { return animationTime; }
        bool AnimationPlaying(const std::string&) const override { return animationPlaying; }
        std::string reloadError;
        int reloads = 0;
        glm::vec3 feet{ 0.0f };
        std::string ReloadLevel() override { ++reloads; return reloadError; }
        std::string requestedLevel;
        void RequestLevel(const std::string& level, const std::string&) override { requestedLevel = level; }
        glm::vec3 FeetPosition() const override { return feet; }
        void Log(const std::string&) override {}
    };

    std::vector<TestCommand> Parse(const char* script)
    {
        TestScriptParseResult result = ParseTestScript(script);
        INFO(result.error);
        REQUIRE(result.error.empty());
        return std::move(result.commands);
    }

    // Steps the runner at 60 fps until it finishes (or a safety cap).
    void Run(TestRunner& runner, FakeGame& game, float seconds = 60.0f)
    {
        for (int frame = 0; frame < static_cast<int>(seconds * 60.0f) && !runner.IsFinished(); ++frame)
        {
            if (game.timeUntilLevelChange >= 0.0f)
            {
                game.timeUntilLevelChange -= 1.0f / 60.0f;
                if (game.timeUntilLevelChange < 0.0f)
                {
                    game.level = "shrine_grounds";
                    game.mode = "exploring";
                    game.arrival = game.arrivalAfterChange;
                }
            }
            if (game.animationPlaying && game.animationTime)
            {
                *game.animationTime += 1.0f / 60.0f;
            }
            runner.Update(1.0f / 60.0f, game);
        }
    }
}

TEST_CASE("Scripts parse, skipping comments and blank lines")
{
    const auto commands = Parse(R"(
        # a comment
        teleport_to torii   # trailing comment

        interact torii
        expect_message You bow
    )");
    REQUIRE(commands.size() == 3);
    CHECK(commands[0].name == "teleport_to");
    CHECK(commands[0].args == std::vector<std::string>{ "torii" });
    CHECK(commands[2].args.size() == 2);
    CHECK(commands[2].line == 6);
}

TEST_CASE("Unknown commands and wrong argument counts are rejected")
{
    CHECK_FALSE(ParseTestScript("fly_to moon").error.empty());
    CHECK_FALSE(ParseTestScript("teleport 1 2").error.empty());
    CHECK_FALSE(ParseTestScript("advance now").error.empty());
    CHECK(ParseTestScript("wait 1\nquit").error.empty());
}

TEST_CASE("A passing script runs every step")
{
    FakeGame game;
    TestRunner runner(Parse(R"(
        teleport_to keeper
        interact keeper
        expect_flag bowed
        expect_message You bow
        expect_voices_max 3
    )"));
    Run(runner, game);

    CHECK(runner.Passed());
    CHECK(game.interactions == 1);
}

TEST_CASE("The first failing expectation stops the script with its line")
{
    FakeGame game;
    TestRunner runner(Parse("interact\nexpect_flag permission\ninteract"));
    Run(runner, game);

    CHECK(runner.IsFinished());
    CHECK_FALSE(runner.Passed());
    CHECK(runner.GetFailure().find("line 2") != std::string::npos);
    CHECK(game.interactions == 1); // line 3 never ran
}

TEST_CASE("Interacting checks that targeting would pick the named entity")
{
    FakeGame game;
    game.target = "vending_machine";
    TestRunner runner(Parse("interact keeper"));
    Run(runner, game);

    CHECK_FALSE(runner.Passed());
    CHECK(game.interactions == 0);
}

TEST_CASE("wait_for_level waits for a transition, and times out otherwise")
{
    FakeGame game;
    game.mode = "transitioning";
    game.timeUntilLevelChange = 1.5f;
    TestRunner waits(Parse("wait_for_level shrine_grounds 5\nexpect_mode exploring"));
    Run(waits, game);
    CHECK(waits.Passed());

    FakeGame stuck;
    TestRunner timesOut(Parse("wait_for_level nowhere 1"));
    Run(timesOut, stuck);
    CHECK_FALSE(timesOut.Passed());
}

TEST_CASE("Entering a level away from its spawn fails the script")
{
    const auto run = [](ArrivalError arrival) {
        FakeGame game;
        game.mode = "transitioning";
        game.timeUntilLevelChange = 0.5f;
        game.arrivalAfterChange = arrival;
        TestRunner runner(Parse("wait_for_level shrine_grounds 5"));
        Run(runner, game);
        return runner.Passed() ? std::string() : runner.GetFailure();
    };

    CHECK(run({ 0.001f, 0.1f }).empty());
    // The v0.0.2 bug: the new level drawn from the old level's position.
    CHECK(run({ 34.2f, 0.0f }).find("shrine_grounds") != std::string::npos);
    CHECK_FALSE(run({ 0.0f, 90.0f }).empty());
}

TEST_CASE("expect_animating needs a clip that advances; wait_for_animation its end")
{
    FakeGame moving;
    moving.animationTime = 0.0f;
    moving.animationPlaying = true;
    TestRunner passes(Parse("expect_animating windmill"));
    Run(passes, moving);
    CHECK(passes.Passed());

    FakeGame still;
    still.animationTime = 2.0f;
    TestRunner fails(Parse("expect_animating windmill"));
    Run(fails, still);
    CHECK_FALSE(fails.Passed());

    FakeGame none;
    TestRunner missing(Parse("expect_animating windmill"));
    Run(missing, none);
    CHECK(missing.GetFailure().find("no animation") != std::string::npos);

    TestRunner done(Parse("wait_for_animation shed"));
    Run(done, still);
    CHECK(done.Passed());

    TestRunner forever(Parse("wait_for_animation windmill 1"));
    Run(forever, moving);
    CHECK_FALSE(forever.Passed());
}

TEST_CASE("reload_level fails with the reload's error; expect_near checks the feet")
{
    FakeGame game;
    game.feet = { 3.0f, 0.0f, 5.0f };
    TestRunner passes(Parse("reload_level\nexpect_near 3 0 5"));
    Run(passes, game);
    CHECK(passes.Passed());
    CHECK(game.reloads == 1);

    TestRunner far(Parse("expect_near 3 0 6 0.5"));
    Run(far, game);
    CHECK_FALSE(far.Passed());

    FakeGame broken;
    broken.reloadError = "street.json:/entities/2/interactable/action/type: unknown action type";
    TestRunner fails(Parse("reload_level"));
    Run(fails, broken);
    CHECK(fails.GetFailure().find("/entities/2") != std::string::npos);
}

TEST_CASE("expect_surface compares the footstep surface underfoot")
{
    FakeGame game;
    game.surface = "wood";
    TestRunner passes(Parse("expect_surface wood"));
    Run(passes, game);
    CHECK(passes.Passed());

    TestRunner fails(Parse("expect_surface dirt"));
    Run(fails, game);
    CHECK_FALSE(fails.Passed());
    CHECK(fails.GetFailure().find("'wood'") != std::string::npos);
}

TEST_CASE("Voice counts above the limit fail as a leak")
{
    FakeGame game;
    game.voices = 12;
    TestRunner runner(Parse("expect_voices_max 7"));
    Run(runner, game);
    CHECK_FALSE(runner.Passed());
    CHECK(runner.GetFailure().find("leak") != std::string::npos);
}

TEST_CASE("wait lasts roughly the requested time")
{
    FakeGame game;
    TestRunner runner(Parse("wait 0.5\nquit"));
    int frames = 0;
    while (!runner.IsFinished() && frames < 600)
    {
        runner.Update(1.0f / 60.0f, game);
        ++frames;
    }
    CHECK(runner.Passed());
    CHECK(frames >= 30);
    CHECK(frames <= 33);
}

TEST_CASE("Docs commands: capture sequences, settings and the panning camera")
{
    FakeGame game;
    TestRunner runner(Parse(R"(
        set msaa 4
        capture shots/spin 3 2
        pan 0 0 0 0 10 0 -4 90 1
    )"));
    for (int frame = 0; frame < 200 && !runner.IsFinished(); ++frame)
    {
        runner.Update(1.0f / 30.0f, game);
    }
    INFO(runner.GetFailure());
    CHECK(runner.Passed());
    CHECK(game.settings == std::vector<std::string>{ "msaa=4" });
    CHECK(game.captures == std::vector<std::string>{ "shots/spin_000", "shots/spin_001", "shots/spin_002" });
    CHECK(game.feet.x == doctest::Approx(10.0f));  // the pan ends exactly at its target
    CHECK(game.feet.z == doctest::Approx(-4.0f));
    CHECK(game.yawDegrees == doctest::Approx(90.0f));

    FakeGame other;
    TestRunner bad(Parse("set nonsense 1\n"));
    bad.Update(1.0f / 30.0f, other);
    CHECK_FALSE(bad.Passed());
}

TEST_CASE("Pan easing starts and stops gently")
{
    CHECK(EasePan(0.0f) == 0.0f);
    CHECK(EasePan(1.0f) == 1.0f);
    CHECK(EasePan(0.5f) == doctest::Approx(0.5f));
    CHECK(EasePan(0.05f) < 0.05f);  // slower than linear at the start
    CHECK(EasePan(0.95f) > 0.95f);  // and at the end
    CHECK(EasePan(2.0f) == 1.0f);
}
