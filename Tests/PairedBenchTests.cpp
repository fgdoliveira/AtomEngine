#include "Testing/PairedBench.h"

#include <doctest/doctest.h>

using namespace AtomGame;

namespace
{
    // A fake machine: B costs `cost` ms more than A, and the whole machine
    // slows by `drift` ms every second (it is heating up).
    struct FakeMachine
    {
        double baseMs = 5.0;
        double cost = 0.4;
        double drift = 0.0;
        bool b = false;
        double seconds = 0.0;

        double Frame()
        {
            const double ms = baseMs + drift * seconds + (b ? cost : 0.0);
            seconds += ms / 1000.0;
            return ms;
        }
    };

    PairedBench Run(FakeMachine& machine, int rounds)
    {
        PairedBench bench(rounds, 1.0, 0.25);
        for (int frame = 0; frame < 1000000; ++frame)
        {
            const PairedBench::Action action = bench.Advance(machine.Frame());
            if (action == PairedBench::Action::SetA) machine.b = false;
            if (action == PairedBench::Action::SetB) machine.b = true;
            if (action == PairedBench::Action::Done) break;
        }
        return bench;
    }
}

TEST_CASE("The bench measures a known cost")
{
    FakeMachine machine;
    const PairedBench bench = Run(machine, 4);
    REQUIRE(bench.GetRounds().size() == 4);
    CHECK(bench.MedianDelta() == doctest::Approx(0.4).epsilon(0.01));
}

TEST_CASE("Rounds alternate AB BA, so a machine heating up doesn't fake a cost")
{
    const PairedBench bench = [] {
        FakeMachine machine;
        machine.cost = 0.0;   // A and B cost the same...
        machine.drift = 0.02; // ...but the machine slows 0.02 ms every second
        return Run(machine, 8);
    }();
    REQUIRE(bench.GetRounds().size() == 8);
    CHECK(bench.GetRounds()[0].aFirst);
    CHECK_FALSE(bench.GetRounds()[1].aFirst);
    // Each round alone is biased by the drift (whichever half ran second
    // looks slower), alternately up and down; the median cancels it.
    CHECK(bench.GetRounds()[0].Delta() > 0.0);
    CHECK(bench.GetRounds()[1].Delta() < 0.0);
    CHECK(std::abs(bench.MedianDelta()) < 0.01);

    // A real cost still shows through the same drift.
    FakeMachine costly;
    costly.drift = 0.02;
    costly.cost = 0.4;
    CHECK(Run(costly, 8).MedianDelta() == doctest::Approx(0.4).epsilon(0.05));
}

TEST_CASE("Settling time is never measured")
{
    // The first frames after a switch are slow (targets being rebuilt):
    // they fall in the settle window and don't count.
    PairedBench bench(2, 0.5, 0.2);
    bool b = false;
    int sinceSwitch = 0;
    for (int frame = 0; frame < 100000; ++frame)
    {
        const double ms = (sinceSwitch < 5 ? 40.0 : 5.0) + (b ? 1.0 : 0.0);
        ++sinceSwitch;
        const PairedBench::Action action = bench.Advance(ms);
        if (action == PairedBench::Action::SetA) { b = false; sinceSwitch = 0; }
        if (action == PairedBench::Action::SetB) { b = true; sinceSwitch = 0; }
        if (action == PairedBench::Action::Done) break;
    }
    REQUIRE(bench.GetRounds().size() == 2);
    CHECK(bench.GetRounds()[0].a == doctest::Approx(5.0));
    CHECK(bench.MedianDelta() == doctest::Approx(1.0));
}
