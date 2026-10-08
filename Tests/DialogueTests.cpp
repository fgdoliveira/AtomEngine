#include "Dialogue/Dialogue.h"
#include "TestAssets.h" // v0.0.14: source assets in two roots
#include "Dialogue/DialogueRunner.h"
#include "World/GameState.h"

#include <doctest/doctest.h>

using namespace AtomGame;

namespace
{
    constexpr const char* Keeper = R"({
        "id": "keeper",
        "start": "greet",
        "nodes": [
            { "id": "greet", "speaker": "Keeper", "text": "Hello.",
              "choices": [
                { "text": "Who are you?", "next": "who" },
                { "text": "Let me in.", "next": "granted", "requires": "bowed" },
                { "text": "Thanks.", "next": "end", "requires": "permission" },
                { "text": "Bye.", "next": "end", "requiresNot": "permission", "sets": "said_bye" }
              ] },
            { "id": "who", "speaker": "Keeper", "text": "The keeper.", "next": "greet" },
            { "id": "granted", "speaker": "Keeper", "text": "Go.", "sets": "permission", "next": "end" }
        ]
    })";

    Dialogue ParseOrFail(const char* json)
    {
        DialogueParseResult result = ParseDialogue(json);
        INFO(result.error);
        REQUIRE(result.dialogue.has_value());
        return std::move(*result.dialogue);
    }

    // Finishes the typewriter so choices can be made.
    void Reveal(DialogueRunner& runner)
    {
        runner.Advance();
        REQUIRE(runner.GetState() == DialogueRunner::State::WaitingForInput);
    }
}

TEST_CASE("A valid dialogue parses with all its nodes")
{
    const Dialogue dialogue = ParseOrFail(Keeper);
    CHECK(dialogue.id == "keeper");
    CHECK(dialogue.nodes.size() == 3);
    REQUIRE(dialogue.Find("greet") != nullptr);
    CHECK(dialogue.Find("greet")->choices.size() == 4);
    CHECK(dialogue.Find("greet")->choices[1].requiredFlag == "bowed");
    CHECK(dialogue.Find("granted")->setsFlag == "permission");
}

TEST_CASE("Broken dialogue data is rejected with a reason")
{
    CHECK_FALSE(ParseDialogue("{ not json").dialogue.has_value());
    CHECK_FALSE(ParseDialogue(R"({ "id": "x", "nodes": [] })").dialogue.has_value());

    const auto missingStart = ParseDialogue(
        R"({ "id": "x", "start": "nope", "nodes": [ { "id": "a", "text": "" } ] })");
    CHECK_FALSE(missingStart.dialogue.has_value());
    CHECK(missingStart.error.find("nope") != std::string::npos);

    const auto badLink = ParseDialogue(
        R"({ "id": "x", "start": "a", "nodes": [ { "id": "a", "text": "", "next": "typo" } ] })");
    CHECK_FALSE(badLink.dialogue.has_value());
    CHECK(badLink.error.find("typo") != std::string::npos);

    const auto duplicate = ParseDialogue(
        R"({ "id": "x", "start": "a", "nodes": [ { "id": "a" }, { "id": "a" } ] })");
    CHECK_FALSE(duplicate.dialogue.has_value());
}

TEST_CASE("The typewriter reveals text over time, and Advance completes it")
{
    const Dialogue dialogue = ParseOrFail(Keeper);
    GameState state;
    DialogueRunner runner;
    runner.charactersPerSecond = 2.0f;
    runner.Start(dialogue, state);

    CHECK(runner.GetState() == DialogueRunner::State::Revealing);
    runner.Update(1.0f);
    CHECK(runner.GetVisibleText() == "He");

    runner.Advance(); // first press shows the whole line
    CHECK(runner.GetVisibleText() == "Hello.");
    CHECK(runner.GetState() == DialogueRunner::State::WaitingForInput);
}

TEST_CASE("Choices are filtered by flags")
{
    const Dialogue dialogue = ParseOrFail(Keeper);
    GameState state;
    DialogueRunner runner;
    runner.Start(dialogue, state);
    Reveal(runner);

    // Not bowed, no permission: "Who are you?" and "Bye." only.
    auto choices = runner.GetVisibleChoices();
    REQUIRE(choices.size() == 2);
    CHECK(choices[0]->text == "Who are you?");
    CHECK(choices[1]->text == "Bye.");

    state.SetFlag("bowed");
    state.SetFlag("permission");
    choices = runner.GetVisibleChoices();
    REQUIRE(choices.size() == 3);
    CHECK(choices[1]->text == "Let me in.");
    CHECK(choices[2]->text == "Thanks.");
}

TEST_CASE("A branch that loops back, then one that sets a flag and ends")
{
    const Dialogue dialogue = ParseOrFail(Keeper);
    GameState state;
    state.SetFlag("bowed");
    DialogueRunner runner;
    runner.Start(dialogue, state);
    Reveal(runner);

    runner.SelectIndex(0); // "Who are you?"
    runner.Confirm();
    REQUIRE(runner.GetNode() != nullptr);
    CHECK(runner.GetNode()->id == "who");

    Reveal(runner);
    runner.Confirm(); // no choices: continue back to greet
    CHECK(runner.GetNode()->id == "greet");

    Reveal(runner);
    runner.SelectIndex(1); // "Let me in." (visible because of "bowed")
    runner.Confirm();
    CHECK(runner.GetNode()->id == "granted");
    CHECK(state.HasFlag("permission")); // set when the node is shown

    Reveal(runner);
    runner.Confirm();
    CHECK(runner.GetState() == DialogueRunner::State::Ended);
    CHECK_FALSE(runner.IsActive());
}

TEST_CASE("Choosing an option can set a flag, and selection wraps")
{
    const Dialogue dialogue = ParseOrFail(Keeper);
    GameState state;
    DialogueRunner runner;
    runner.Start(dialogue, state);
    Reveal(runner);

    runner.MoveSelection(-1); // wraps from the first to the last visible choice
    CHECK(runner.GetSelection() == 1);
    runner.Confirm(); // "Bye."
    CHECK(state.HasFlag("said_bye"));
    CHECK(runner.GetState() == DialogueRunner::State::Ended);
}

TEST_CASE("The shipped shrine keeper dialogue is valid")
{
    const DialogueParseResult result =
        LoadDialogueFile(AtomTests::Asset("Dialogue/shrine_keeper.json"));
    INFO(result.error);
    REQUIRE(result.dialogue.has_value());
    CHECK(result.dialogue->id == "shrine_keeper");

    // The permission branch exists and grants the flag.
    bool grants = false;
    for (const auto& [id, node] : result.dialogue->nodes)
    {
        grants = grants || node.setsFlag == "keeper_permission";
    }
    CHECK(grants);
}
