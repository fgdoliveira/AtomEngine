#include "Interaction/Sequence.h"
#include "Level/LevelData.h"

#include <doctest/doctest.h>

#include <string>
#include <vector>

using namespace AtomGame;

namespace
{
    // Records what a sequence does, with a movable entity at `position`.
    struct Recorder
    {
        std::vector<std::string> log;
        glm::vec3 position{ 100.0f, 0.0f, 0.0f };
        SequenceHooks Hooks()
        {
            return SequenceHooks{
                [this](const std::string& text) { log.push_back("message " + text); },
                [this](const std::string& flag) { log.push_back("flag " + flag); },
                [this](const std::string& entity, bool visible) { log.push_back((visible ? "show " : "hide ") + entity); },
                [this](const std::string& sound, const std::string&, float, bool) { log.push_back("sound " + sound); },
                [this](const std::string&, const std::string& clip) { log.push_back("anim " + clip); return true; },
                [this](const std::string&) { return std::optional<glm::vec3>(position); },
                [this](const std::string&, const glm::vec3& p) { position = p; },
                [this](const std::string& level, const std::string& spawn) { log.push_back("level " + level + " " + spawn); },
            };
        }
    };

    SequenceStep Step(SequenceStep::Type type, float seconds = 0.0f, std::string text = {})
    {
        SequenceStep step;
        step.type = type;
        step.seconds = seconds;
        step.text = std::move(text);
        step.entity = "bus";
        return step;
    }

    std::string Level(const std::string& body)
    {
        return R"({ "name": "t", "model": "m.glb", "collision": "c.glb", "spawns": { "a": { "position": [0,0,0] } }, )" + body + "}";
    }
}

TEST_CASE("Sequences run their steps in order, in time, once")
{
    using Type = SequenceStep::Type;
    SequenceStep move = Step(Type::MoveEntity, 2.0f);
    move.to = { 0.0f, 0.0f, 0.0f };
    const Sequence bus{ Step(Type::Message, 0, "waiting"), Step(Type::Wait, 1.0f), Step(Type::Show), move,
                        Step(Type::SetFlag, 0, "rode"), Step(Type::ChangeLevel, 0, "city") };
    SequenceRunner runner;
    Recorder recorder;
    const SequenceHooks hooks = recorder.Hooks();

    REQUIRE(runner.Start(bus, "bus"));
    CHECK_FALSE(runner.Start(bus, "bus")); // can't start twice

    runner.Update(0.5f, hooks);
    CHECK(recorder.log == std::vector<std::string>{ "message waiting" }); // waiting
    runner.Update(0.6f, hooks);                                          // the wait ends; 0.1 s into the drive
    CHECK(recorder.log.back() == "show bus");
    CHECK(recorder.position.x < 100.0f);
    CHECK(recorder.position.x > 0.0f);
    const float early = 100.0f - recorder.position.x;
    runner.Update(0.1f, hooks);
    const float later = 100.0f - recorder.position.x - early;
    CHECK(later < early * 1.1f); // eases out: it never speeds up
    CHECK(runner.IsRunning());

    runner.Update(5.0f, hooks); // arrives, then everything instant runs
    CHECK(recorder.position.x == doctest::Approx(0.0f));
    CHECK(recorder.log.back() == "level city ");
    CHECK(recorder.log[recorder.log.size() - 2] == "flag rode");
    CHECK_FALSE(runner.IsRunning());
    CHECK(runner.Start(bus, "bus")); // free again
}

TEST_CASE("Sequences parse from the level; names and order are checked")
{
    const auto ok = ParseLevel(Level(R"(
        "entities": [ { "name": "bus", "position": [0,0,0], "hidden": true },
                      { "name": "stop", "position": [0,0,0], "interactable": { "action": { "type": "sequence", "id": "ride" } } } ],
        "sequences": { "ride": [ { "type": "wait", "seconds": 2 },
                                 { "type": "show", "entity": "bus" },
                                 { "type": "playSound", "sound": "bus_engine", "entity": "bus", "loop": true },
                                 { "type": "moveEntity", "entity": "bus", "to": [1,0,0], "seconds": 8 },
                                 { "type": "changeLevel", "level": "street", "spawn": "from_bus" } ] })"));
    INFO(ok.error);
    REQUIRE(ok.level.has_value());
    CHECK(ok.level->entities[0].hidden);
    const Sequence& ride = ok.level->sequences.at("ride");
    REQUIRE(ride.size() == 5);
    CHECK(ride[2].loop);
    CHECK(ride[3].to.x == doctest::Approx(1.0f));
    CHECK(ride[4].text == "street");
    CHECK(ride[4].clip == "from_bus");

    const auto unknownSequence = ParseLevel(Level(R"(
        "entities": [ { "name": "stop", "position": [0,0,0], "interactable": { "action": { "type": "sequence", "id": "nope" } } } ])"));
    CHECK(unknownSequence.error.rfind("/entities/0/interactable:", 0) == 0);
    const auto unknownEntity = ParseLevel(Level(R"("sequences": { "s": [ { "type": "show", "entity": "ghost" } ] })"));
    CHECK(unknownEntity.error.rfind("/sequences/s/0/entity:", 0) == 0);
    const auto notLast = ParseLevel(Level(R"("sequences": { "s": [ { "type": "changeLevel", "level": "x" }, { "type": "wait", "seconds": 1 } ] })"));
    CHECK(notLast.error.rfind("/sequences/s/0:", 0) == 0);
    const auto badStep = ParseLevel(Level(R"("sequences": { "s": [ { "type": "fly" } ] })"));
    CHECK(badStep.error.rfind("/sequences/s/0/type:", 0) == 0);
    const auto noSeconds = ParseLevel(Level(R"("sequences": { "s": [ { "type": "wait" } ] })"));
    CHECK(noSeconds.error.rfind("/sequences/s/0:", 0) == 0);
}
