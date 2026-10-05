#include "Platform/RunLog.h"

#include <doctest/doctest.h>

#include <ostream>
#include <sstream>

using namespace AtomGame;

TEST_CASE("TeeBuffer: both outputs receive everything")
{
    std::ostringstream console;
    std::ostringstream file;
    TeeBuffer tee(console.rdbuf(), file.rdbuf());
    std::ostream out(&tee);
    out << "GPU device created" << '\n' << 42 << std::endl;
    CHECK(console.str() == "GPU device created\n42\n");
    CHECK(file.str() == console.str());
}

TEST_CASE("TeeBuffer: a missing output is skipped (no console, or no log file)")
{
    std::ostringstream file;
    TeeBuffer tee(nullptr, file.rdbuf());
    std::ostream out(&tee);
    out << "only the log\n";
    CHECK(file.str() == "only the log\n");
}

TEST_CASE("TeeBuffer: the last complete line is kept for the failure message")
{
    TeeBuffer tee(nullptr, nullptr);
    std::ostream out(&tee);
    out << "first\r\n";
    CHECK(tee.LastLine() == "first");
    out << "No GPU could present to this window.\n\npartial";
    CHECK(tee.LastLine() == "No GPU could present to this window."); // blank lines and partial ones don't replace it
}
