#pragma once

#include "ArenaSim.h"

#include <optional>
#include <string>
#include <vector>

namespace Arena
{
    // A named, fixed workload (M94): which parts run and how big they are.
    // Each isolates one workload so it can be measured apart:
    //   update_N         A only: N enemies steer; no collision, no drawing
    //   collision_N      A + B with the grid search, no drawing
    //   collision_naive_N  the same with the all-pairs search
    //   projectiles_N    250 enemies + N projectiles, colliding (grid), no drawing
    //   spike_N          250 enemies; every 300 frames N more arrive at once,
    //                    150 frames later they go
    //   draw_N           N enemies drawn (one draw each), barely simulated
    //   full_N           everything at once: N enemies, 2N projectiles, drawn
    struct BenchmarkScenario
    {
        std::string name;
        ArenaConfig config;
        bool drawEnemies = false;
        int spikeBatch = 0;      // 0: no spike
        int spikePeriod = 300;   // frames between spikes
        int spikeLength = 150;   // frames the batch stays
    };
    std::optional<BenchmarkScenario> FindScenario(const std::string& name);
    // The matrix Tools/Perf/arena.ps1 runs by default.
    std::vector<std::string> StandardScenarios();

    // --benchmark --scenario <name> [--seed N] [--frames N] [--warmup N] [--out file]
    struct BenchmarkOptions
    {
        bool benchmark = false;
        std::string scenario = "full_250";
        std::uint32_t seed = 42;
        int frames = 1800;  // measured
        int warmup = 120;   // run first, not measured
        std::string out;    // "" : out/arena/<scenario>-<date>-<time>.txt
        std::vector<std::string> errors;
    };
    BenchmarkOptions ParseBenchmarkArguments(const std::vector<std::string>& arguments);
}
