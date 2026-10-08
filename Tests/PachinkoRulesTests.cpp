#include "Pachinko/PachinkoGame.h"
#include "TestAssets.h" // v0.0.14: source assets in two roots
#include "Pachinko/PachinkoRules.h"
#include "Pachinko/Playfield.h"

#include <doctest/doctest.h>

#include <algorithm>
#include <vector>

using namespace AtomGame;

namespace
{
    constexpr float Tick = 1.0f / 60.0f;

    using Event = PachinkoRules::Event;
    using State = PachinkoRules::State;

    // Runs until `done` or `seconds` pass; collects every event.
    template <typename Done>
    void RunUntil(PachinkoRules& rules, std::vector<Event>& events, float seconds, Done done)
    {
        for (float t = 0.0f; t < seconds && !done(); t += Tick)
        {
            rules.Update(Tick);
            events.insert(events.end(), rules.GetEvents().begin(), rules.GetEvents().end());
        }
    }

    bool Has(const std::vector<Event>& events, Event event)
    {
        return std::find(events.begin(), events.end(), event) != events.end();
    }

    Playfield Shipped()
    {
        const PlayfieldParseResult result = LoadPlayfieldFile(AtomTests::Asset("Machines/night_fever.json"));
        INFO(result.error);
        REQUIRE(result.playfield.has_value());
        return *result.playfield;
    }
}

TEST_CASE("Rules: a hit goes through the reach, the result and every fever round")
{
    RulesSettings settings;
    settings.odds = 1; // always hit
    settings.feverRounds = 3;
    settings.ballsPerRound = 2;
    PachinkoRules rules(settings, 1);
    std::vector<Event> events;

    rules.OnStartPocket();
    rules.Update(Tick);
    CHECK(rules.GetState() == State::Spinning);
    CHECK(rules.GetHeld() == 0);
    RunUntil(rules, events, 10.0f, [&] { return rules.GetState() == State::Round; });
    CHECK(Has(events, Event::Reach));
    CHECK(Has(events, Event::Hit));
    CHECK(Has(events, Event::FeverStart));
    CHECK(rules.IsGateOpen());
    const auto& reels = rules.GetReels();
    CHECK((reels[0] == reels[1] && reels[1] == reels[2])); // three of a kind

    for (int round = 1; round <= 3; ++round)
    {
        CHECK(rules.GetRound() == round);
        rules.OnAttacker();
        rules.OnAttacker(); // the round's balls: it closes
        events.clear();
        RunUntil(rules, events, 5.0f, [&] { return rules.GetState() != State::Round; });
        CHECK(Has(events, Event::RoundEnd));
        CHECK_FALSE(rules.IsGateOpen());
        if (round < 3)
        {
            RunUntil(rules, events, 5.0f, [&] { return rules.GetState() == State::Round; });
        }
    }
    CHECK(Has(events, Event::FeverEnd));
    CHECK(rules.GetState() == State::Idle);
}

TEST_CASE("Rules: a round also ends on its time limit")
{
    RulesSettings settings;
    settings.odds = 1;
    settings.feverRounds = 1;
    settings.roundSeconds = 2.0f;
    PachinkoRules rules(settings, 2);
    std::vector<Event> events;
    rules.OnStartPocket();
    RunUntil(rules, events, 10.0f, [&] { return rules.GetState() == State::Round; });
    RunUntil(rules, events, 2.5f, [&] { return rules.GetState() != State::Round; });
    CHECK(Has(events, Event::FeverEnd)); // no balls went in; time ran out
}

TEST_CASE("Rules: misses, reaches, and held spins capped")
{
    RulesSettings settings;
    settings.odds = 1000000; // never hit
    settings.reachChance = 1.0f; // every miss teases
    PachinkoRules rules(settings, 3);
    for (int i = 0; i < 6; ++i)
    {
        rules.OnStartPocket();
    }
    CHECK(rules.GetHeld() == settings.maxHeld);
    std::vector<Event> events;
    rules.Update(Tick);
    RunUntil(rules, events, 10.0f, [&] { return rules.GetState() == State::Result; });
    CHECK(Has(events, Event::Reach));
    CHECK(Has(events, Event::Miss));
    const auto& reels = rules.GetReels();
    CHECK(reels[0] == reels[1]);   // the reach...
    CHECK(reels[2] != reels[0]);   // ...and the miss by one
    // The held spins play one after another.
    RunUntil(rules, events, 60.0f, [&] { return rules.GetHeld() == 0 && rules.GetState() == State::Idle; });
    CHECK(rules.GetSpins() == static_cast<std::uint32_t>(settings.maxHeld));
}

TEST_CASE("Rules: the hit rate matches the odds over many draws")
{
    RulesSettings settings;
    settings.odds = 99;
    PachinkoRules rules(settings, 42);
    int hits = 0;
    constexpr int Draws = 100000;
    for (int i = 0; i < Draws; ++i)
    {
        hits += rules.DrawHit() ? 1 : 0;
    }
    const double expected = Draws / 99.0; // ~1010
    CHECK(hits > expected * 0.88);        // within ~4 standard deviations
    CHECK(hits < expected * 1.12);
}

TEST_CASE("A whole session replays exactly from its seed and inputs")
{
    const Playfield field = Shipped();
    const auto session = [&] {
        PachinkoGame game(field, 9);
        game.AddToTray(400);
        for (int tick = 0; tick < 60 * 90; ++tick)
        {
            // A player who plays in bursts and fiddles with the knob.
            const bool launch = (tick / 300) % 3 != 2;
            const float knob = (tick % 700 < 20) ? 0.01f : (tick % 900 < 15 ? -0.01f : 0.0f);
            game.Step({ launch, knob });
        }
        return std::tuple{ game.GetTray(), game.GetLaunched(), game.GetRules().GetSpins(), game.GetCaught(Pocket::Kind::Out) };
    };
    CHECK(session() == session());
}

TEST_CASE("With the odds forced, a fever opens the gate and pays")
{
    Playfield field = Shipped();
    field.rules.odds = 1;
    PachinkoGame game(field, 4);
    game.AddToTray(600);
    game.SetStrength(0.5f);
    bool opened = false;
    for (int tick = 0; tick < 60 * 120; ++tick)
    {
        game.Step({ true, 0.0f });
        opened = opened || game.IsGateOpen();
    }
    CHECK(opened);
    CHECK(game.GetRules().GetHits() >= 1);
    CHECK(game.GetCaught(Pocket::Kind::Attacker) > 0); // balls went through the open gate
}
