// The engine's log lines (v0.0.15, M100): a time, a level, the part of the
// engine that speaks, then the message - and a player's message box shows
// only the message.
#include "Core/Log.h"
#include "Platform/RunLog.h"

#include <doctest/doctest.h>

using namespace Atom;

TEST_CASE("A log line: time, level, subsystem, message - one line, its newline dropped")
{
    const std::int64_t ms = ((14 * 60 + 3) * 60 + 7) * 1000 + 412; // 14:03:07.412
    CHECK(Log::Format(Log::Level::Error, "Renderer", "Failed to begin the pass\n", ms)
          == "[14:03:07.412] [ERROR] [Renderer] Failed to begin the pass");
    CHECK(Log::Format(Log::Level::Info, "Audio", "Mixer ready", 0) == "[00:00:00.000] [INFO ] [Audio] Mixer ready");
    CHECK(Log::Format(Log::Level::Warn, "GPU", "x", 86'400'000 + 1) == "[00:00:00.001] [WARN ] [GPU] x"); // wraps a day
    // The level names are one width, so the columns line up.
    CHECK(Log::Name(Log::Level::Info).size() == Log::Name(Log::Level::Error).size());
    CHECK(Log::Name(Log::Level::Warn).size() == Log::Name(Log::Level::Error).size());
}

TEST_CASE("A start-failure message shows the message without the log prefix")
{
    CHECK(AtomFramework::WithoutLogPrefix("[14:03:07.412] [ERROR] [GPU] No GPU device could be created")
          == "No GPU device could be created");
    CHECK(AtomFramework::WithoutLogPrefix("A plain line, from a game") == "A plain line, from a game");
    CHECK(AtomFramework::WithoutLogPrefix("[only] [two] groups") == "[only] [two] groups");
}
