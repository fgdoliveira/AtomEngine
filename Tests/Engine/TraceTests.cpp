// The minimal tracer (v0.0.15, M96; ADR-010). Zones nest, a full buffer
// stops recording rather than
// growing, and the summaries and exports say what was recorded.
#include "Core/Trace.h"

#include <doctest/doctest.h>

#include <string>

using namespace Atom;

TEST_CASE("Zones record with their depth and frame; nothing is recorded when not started")
{
    Trace::Reset();
    {
        Trace::Scope ignored("Ignored");
    }
    CHECK(Trace::Events().empty());

    Trace::Start(16);
    {
        Trace::Scope outer("Frame");
        {
            Trace::Scope inner("Update");
        }
    }
    Trace::FrameMark();
    {
        Trace::Scope next("Frame");
    }
    REQUIRE(Trace::Events().size() == 3);
    CHECK(std::string(Trace::Events()[0].name) == "Frame");
    CHECK(Trace::Events()[0].depth == 0);
    CHECK(Trace::Events()[1].depth == 1);
    CHECK(Trace::Events()[1].frame == 0);
    CHECK(Trace::Events()[2].frame == 1);
    CHECK(Trace::Events()[0].endNs >= Trace::Events()[1].endNs); // the outer zone ends last
    CHECK(Trace::Frames() == 1);
    Trace::Reset();
}

TEST_CASE("A full buffer stops recording instead of growing")
{
    Trace::Start(2);
    for (int i = 0; i < 5; ++i)
    {
        Trace::Scope zone("Z");
    }
    CHECK(Trace::Events().size() == 2);
    CHECK(Trace::Overflowed());
    CHECK_FALSE(Trace::IsRecording());
    Trace::Reset();
}

TEST_CASE("The summary, the Chrome trace and the slowest frames say what was recorded")
{
    const std::vector<Trace::Event> events = {
        { "Frame", 0, 10'000'000, 0, 0 },
        { "Collision", 1'000'000, 4'000'000, 0, 1 },
        { "Frame", 10'000'000, 12'000'000, 1, 0 },
        { "Collision", 10'500'000, 11'000'000, 1, 1 },
    };
    const auto stats = Trace::Summarise(events);
    REQUIRE(stats.size() == 2);
    CHECK(stats[0].name == "Frame"); // by total time, largest first
    CHECK(stats[0].count == 2);
    CHECK(stats[0].totalNs == 12'000'000);
    CHECK(stats[1].minNs == 500'000);
    CHECK(stats[1].maxNs == 3'000'000);

    const std::string json = Trace::ChromeJson(events);
    CHECK(json.find("\"traceEvents\"") != std::string::npos);
    CHECK(json.find("\"name\":\"Collision\",\"ph\":\"X\",\"ts\":1000.000,\"dur\":3000.000") != std::string::npos);

    const std::string slowest = Trace::SlowestFrames(events, 1);
    CHECK(slowest.rfind("frame 0: 10.000 ms", 0) == 0); // frame 0 is the slow one
    CHECK(slowest.find("Collision") != std::string::npos);
    CHECK(slowest.find("frame 1") == std::string::npos);
}
