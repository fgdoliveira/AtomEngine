#include "FakeGame.h"
#include "Testing/GameDiagnostics.h"

#include <doctest/doctest.h>

#include <sstream>
#include <string>

using namespace AtomGame;
using AtomGameTests::FakeGame;

// Characterization tests (M57): what the frame-time log and the scripted
// tests did inside DemoApp, they still do in GameDiagnostics - the PERF line
// is byte for byte what ab.ps1 and the bench docs parse.

TEST_CASE("The PERF line keeps its exact format")
{
    Atom::FrameStatsWindow window;
    for (double ms : { 6.0, 6.2, 6.4, 6.6, 9.0 })
    {
        window.AddSample(ms);
    }
    CHECK(GameDiagnostics::FormatPerfLine(12, window, "night_street")
          == "PERF block 12 samples 5 median 6.400 p95 8.520 mean 6.840 label night_street");
}

TEST_CASE("The frame-time log warms up, then reports one line (and CSV row) per block")
{
    std::ostringstream out;
    std::ostringstream csv;
    GameDiagnostics diagnostics;
    diagnostics.ConfigurePerf({ .enabled = true, .block = 4, .warmup = 2 }, out, &csv);

    for (int i = 0; i < 2; ++i)
    {
        diagnostics.RecordFrameTime(0.100f, "street"); // warm-up: not counted
    }
    CHECK(out.str().empty());
    for (int i = 0; i < 8; ++i)
    {
        diagnostics.RecordFrameTime(0.005f, "street");
    }
    CHECK(out.str() == "PERF block 0 samples 4 median 5.000 p95 5.000 mean 5.000 label street\n"
                       "PERF block 1 samples 4 median 5.000 p95 5.000 mean 5.000 label street\n");
    CHECK(csv.str().rfind("block,samples,median_ms,p95_ms,mean_ms,label\n0,4,", 0) == 0);
    CHECK(diagnostics.RealFrameMs() == doctest::Approx(5.0));

    // Off: nothing written, the real frame time still known (bench uses it).
    std::ostringstream quiet;
    GameDiagnostics off;
    off.ConfigurePerf({}, quiet);
    off.RecordFrameTime(0.016f, "street");
    CHECK(quiet.str().empty());
    CHECK(off.RealFrameMs() == doctest::Approx(16.0));
}

TEST_CASE("The fixed step replaces the real one only when set")
{
    GameDiagnostics diagnostics;
    CHECK(diagnostics.Step(0.021f) == doctest::Approx(0.021f));
    diagnostics.SetFixedStep(1.0f / 30.0f);
    CHECK(diagnostics.Step(0.021f) == doctest::Approx(1.0f / 30.0f));
    diagnostics.SetFixedStep(0.0f);
    CHECK(diagnostics.Step(0.021f) == doctest::Approx(0.021f));
}

TEST_CASE("A scripted test reports its exit code once: 0 passed, 1 failed")
{
    FakeGame game;
    {
        GameDiagnostics diagnostics;
        CHECK_FALSE(diagnostics.HasTestScript());
        CHECK_FALSE(diagnostics.UpdateTestScript(0.016f, game).has_value()); // no script: nothing
        diagnostics.SetTestScript(ParseTestScript("expect_level street\n").commands);
        CHECK(diagnostics.HasTestScript());
        std::optional<int> code;
        for (int frame = 0; frame < 10 && !code; ++frame)
        {
            code = diagnostics.UpdateTestScript(0.016f, game);
        }
        REQUIRE(code.has_value());
        CHECK(*code == 0);
        CHECK_FALSE(diagnostics.UpdateTestScript(0.016f, game).has_value()); // reported once
    }
    {
        GameDiagnostics diagnostics;
        diagnostics.SetTestScript(ParseTestScript("expect_level passage\n").commands);
        std::optional<int> code;
        for (int frame = 0; frame < 10 && !code; ++frame)
        {
            code = diagnostics.UpdateTestScript(0.016f, game);
        }
        REQUIRE(code.has_value());
        CHECK(*code == 1);
    }
}
