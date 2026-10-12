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
    CHECK(Log::Name(Log::Level::Game).size() == Log::Name(Log::Level::Error).size());
    // A game's own event (M100): its own level, tagged with the game.
    CHECK(Log::Format(Log::Level::Game, "Demo", "Unease: figure at (-36, 0.8)\n", 0)
          == "[00:00:00.000] [GAME ] [Demo] Unease: figure at (-36, 0.8)");
}

TEST_CASE("A start-failure message shows the message without the log prefix")
{
    CHECK(AtomFramework::WithoutLogPrefix("[14:03:07.412] [ERROR] [GPU] No GPU device could be created")
          == "No GPU device could be created");
    CHECK(AtomFramework::WithoutLogPrefix("A plain line, from a game") == "A plain line, from a game");
    CHECK(AtomFramework::WithoutLogPrefix("[only] [two] groups") == "[only] [two] groups");
}

TEST_CASE("Paths in log lines read relative to the registered folders")
{
    const std::vector<std::string> roots = { "C:/Games/AtomEngine/build/bin/Debug/", "C:/Games/AtomEngine/" };
    // Either separator, any case; the longer root wins.
    CHECK(Log::Relative("Loaded model 'C:\\Games\\AtomEngine\\build\\bin\\Debug\\Assets/Kit/torii.glb'", roots)
          == "Loaded model 'Assets/Kit/torii.glb'");
    CHECK(Log::Relative("Reading c:/games/atomengine/Content/Kit", roots) == "Reading Content/Kit");
    // A folder only matches whole: no root, no change.
    CHECK(Log::Relative("C:/Games/AtomEngineX/file", { "C:/Games/AtomEngine/" }) == "C:/Games/AtomEngineX/file");
    CHECK(Log::Relative("no paths here", roots) == "no paths here");
}
