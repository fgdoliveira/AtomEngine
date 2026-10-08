#include "Pachinko/PachinkoGame.h"
#include "TestAssets.h" // v0.0.14: source assets in two roots
#include "Pachinko/Playfield.h"

#include <doctest/doctest.h>

#include <cstdio>
#include <string>

using namespace AtomGame;

namespace
{
    Playfield Shipped()
    {
        const PlayfieldParseResult result = LoadPlayfieldFile(AtomTests::Asset("Machines/night_fever.json"));
        INFO(result.error);
        REQUIRE(result.playfield.has_value());
        return *result.playfield;
    }

    // Fires `balls` at one strength and plays until the board is empty.
    PachinkoGame Play(const Playfield& field, float strength, int balls, std::uint32_t seed = 1)
    {
        PachinkoGame game(field, seed);
        game.AddToTray(balls);
        game.SetStrength(strength);
        int ticks = 0;
        while ((game.GetLaunched() < static_cast<std::uint32_t>(balls) || game.GetBallsInPlay() > 0) && ticks < 60 * 600)
        {
            game.Step({ game.GetLaunched() < static_cast<std::uint32_t>(balls), 0.0f });
            ++ticks;
        }
        return game;
    }
}

TEST_CASE("The shipped machine loads, and every ball ends up somewhere")
{
    const Playfield field = Shipped();
    CHECK(field.nails.size() > 80);
    for (const float strength : { 0.0f, 0.3f, 0.5f, 0.7f, 1.0f })
    {
        const PachinkoGame game = Play(field, strength, 200);
        std::printf("strength %.1f: foul %u start %u side %u out %u, left in play %zu\n", strength,
                    game.GetCaught(Pocket::Kind::Foul), game.GetCaught(Pocket::Kind::Start),
                    game.GetCaught(Pocket::Kind::Side), game.GetCaught(Pocket::Kind::Out), game.GetBallsInPlay());
        for (const Ball& ball : game.GetWorld().GetBalls())
        {
            std::printf("  stuck at (%.1f, %.1f)\n", ball.position.x, ball.position.y);
        }
        CHECK(game.GetBallsInPlay() == 0); // nothing stuck on the board
        const std::uint32_t accounted = game.GetCaught(Pocket::Kind::Foul) + game.GetCaught(Pocket::Kind::Start)
            + game.GetCaught(Pocket::Kind::Side) + game.GetCaught(Pocket::Kind::Out) + game.GetCaught(Pocket::Kind::Attacker);
        CHECK(accounted == 200);
    }
}

TEST_CASE("Playfield files: errors name the value")
{
    const auto missing = ParsePlayfield(R"({ "field": { "min": [0,0], "max": [100,100] },
        "launch": { "position": [10,10], "direction": [0,-1] }, "pockets": [] })");
    CHECK(missing.error.rfind("/pockets:", 0) == 0);
    const auto outside = ParsePlayfield(R"({ "field": { "min": [0,0], "max": [100,100] }, "walls": [[0,0,200,0]],
        "launch": { "position": [10,10], "direction": [0,-1] },
        "pockets": [ { "kind": "start", "min": [0,0], "max": [5,5] }, { "kind": "out", "min": [0,90], "max": [100,100] } ] })");
    CHECK(outside.error.rfind("/walls:", 0) == 0);
    const auto kind = ParsePlayfield(R"({ "field": { "min": [0,0], "max": [100,100] },
        "launch": { "position": [10,10], "direction": [0,-1] }, "pockets": [ { "kind": "jackpot", "min": [0,0], "max": [5,5] } ] })");
    CHECK(kind.error.rfind("/pockets/0/kind:", 0) == 0);
}
