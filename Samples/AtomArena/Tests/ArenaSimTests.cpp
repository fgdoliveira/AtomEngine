// ATOM ARENA's simulation (v0.0.15, M93): seeded, and the same whichever
// collision search finds the contacts - so a measurement compares the
// searches, never two different simulations.
#include "ArenaSim.h"

#include <doctest/doctest.h>

#include <glm/geometric.hpp>

using namespace Arena;

namespace
{
    ArenaConfig Busy(CollisionMode mode, std::uint32_t seed = 42)
    {
        ArenaConfig config;
        config.seed = seed;
        config.enemies = 300;
        config.projectiles = 200;
        config.collision = mode;
        return config;
    }

    std::uint64_t Run(const ArenaConfig& config, int steps)
    {
        ArenaSim sim(config);
        for (int i = 0; i < steps; ++i)
        {
            sim.Step(1.0f / 60.0f);
        }
        return sim.StateHash();
    }
}

TEST_CASE("The same seed and steps give the same arena, bit for bit; another seed doesn't")
{
    CHECK(Run(Busy(CollisionMode::Grid), 300) == Run(Busy(CollisionMode::Grid), 300));
    CHECK(Run(Busy(CollisionMode::Grid), 300) != Run(Busy(CollisionMode::Grid, 7), 300));
}

TEST_CASE("The grid finds exactly the contacts the naive search finds")
{
    ArenaSim naive(Busy(CollisionMode::Naive));
    ArenaSim grid(Busy(CollisionMode::Grid));
    for (int step = 0; step < 240; ++step)
    {
        naive.Step(1.0f / 60.0f);
        grid.Step(1.0f / 60.0f);
        if (step % 40 == 0)
        {
            std::vector<std::pair<int, int>> a, b;
            naive.FindEnemyContacts(a);
            grid.FindEnemyContacts(b);
            CHECK(a == b);
            naive.FindProjectileHits(a);
            grid.FindProjectileHits(b);
            CHECK(a == b);
        }
    }
    // Same contacts, resolved in the same order: the same simulation.
    CHECK(naive.StateHash() == grid.StateHash());
    CHECK(naive.TotalHits() > 0); // the projectiles do hit
}

TEST_CASE("Enemies close in on the target and stay on the floor")
{
    ArenaConfig config;
    config.enemies = 200;
    ArenaSim sim(config);
    float before = 0.0f;
    for (const Enemy& e : sim.Enemies()) before += glm::length(e.position);
    for (int i = 0; i < 300; ++i)
    {
        sim.Step(1.0f / 60.0f);
    }
    float after = 0.0f;
    for (const Enemy& e : sim.Enemies())
    {
        after += glm::length(e.position);
        CHECK(std::abs(e.position.x) <= config.arenaHalf - config.enemyRadius + 1e-4f);
        CHECK(std::abs(e.position.y) <= config.arenaHalf - config.enemyRadius + 1e-4f);
    }
    CHECK(after < before * 0.7f);
    CHECK_FALSE(sim.EnemyContacts().empty()); // crowded at the centre: they touch
}

TEST_CASE("Each workload switches on its own")
{
    ArenaConfig config;
    config.enemies = 50;
    config.projectiles = 50;
    config.updateEnemies = false;
    config.collide = false;
    ArenaSim sim(config);
    const auto enemiesBefore = sim.Enemies();
    const auto projectilesBefore = sim.Projectiles();
    sim.Step(1.0f / 60.0f);
    CHECK(sim.Enemies()[10].position == enemiesBefore[10].position);       // A off: still
    CHECK(sim.Projectiles()[10].position != projectilesBefore[10].position); // C on: flying
    CHECK(sim.EnemyContacts().empty());                                      // B off: none looked for
}

TEST_CASE("The spike: a batch of enemies arrives at once, at the edge")
{
    ArenaSim sim;
    const std::size_t before = sim.Enemies().size();
    sim.SpawnBatch(500);
    REQUIRE(sim.Enemies().size() == before + 500);
    const Enemy& last = sim.Enemies().back();
    const float edge = sim.Config().arenaHalf - sim.Config().enemyRadius;
    CHECK((std::abs(std::abs(last.position.x) - edge) < 1e-4f || std::abs(std::abs(last.position.y) - edge) < 1e-4f));
}

// M94: the benchmark's scenarios and command line.
#include "ArenaBenchmark.h"

TEST_CASE("A benchmark scenario name says which workload and how big")
{
    const auto collision = FindScenario("collision_500");
    REQUIRE(collision);
    CHECK(collision->config.enemies == 500);
    CHECK(collision->config.collide);
    CHECK(collision->config.collision == CollisionMode::Grid);
    CHECK_FALSE(collision->drawEnemies);
    CHECK(FindScenario("collision_naive_500")->config.collision == CollisionMode::Naive);
    CHECK_FALSE(FindScenario("update_250")->config.collide);
    CHECK(FindScenario("projectiles_1000")->config.projectiles == 1000);
    CHECK(FindScenario("spike_500")->spikeBatch == 500);
    CHECK(FindScenario("draw_100")->drawEnemies);
    CHECK_FALSE(FindScenario("bogus_10"));
    CHECK_FALSE(FindScenario("collision"));
    CHECK_FALSE(FindScenario("collision_x"));
    for (const std::string& name : StandardScenarios())
    {
        INFO(name);
        CHECK(FindScenario(name));
    }
}

TEST_CASE("The benchmark's command line: values, defaults and errors")
{
    const BenchmarkOptions options = ParseBenchmarkArguments(
        { "--benchmark", "--scenario", "collision_500", "--seed", "7", "--frames", "900", "--warmup", "60" });
    CHECK(options.errors.empty());
    CHECK(options.benchmark);
    CHECK(options.scenario == "collision_500");
    CHECK(options.seed == 7u);
    CHECK(options.frames == 900);
    CHECK(options.warmup == 60);
    CHECK(ParseBenchmarkArguments({}).errors.empty()); // no arguments: just play
    CHECK_FALSE(ParseBenchmarkArguments({ "--benchmark", "--scenario", "nope_1" }).errors.empty());
    CHECK_FALSE(ParseBenchmarkArguments({ "--frames", "many" }).errors.empty());
    CHECK_FALSE(ParseBenchmarkArguments({ "--what" }).errors.empty());
}

TEST_CASE("The spike's end: the batch goes, the rest stay")
{
    ArenaSim sim;
    sim.SpawnBatch(400);
    sim.Step(1.0f / 60.0f);
    sim.Truncate(100);
    CHECK(sim.Enemies().size() == 100);
    CHECK(sim.EnemyContacts().empty()); // no pair names a removed enemy
    sim.Step(1.0f / 60.0f);             // and the next step is fine
}
