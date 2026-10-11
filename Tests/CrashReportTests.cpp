// Why it stopped (v0.0.15, M101): a crash's report names the exception and
// its files sort by time; memory reads as one line.
#include "Platform/CrashReport.h"
#include "Platform/Memory.h"

#include <doctest/doctest.h>

using namespace Atom;

TEST_CASE("A crash report names the exception, and its files sort by time")
{
    CHECK(CrashReport::ExceptionName(0xC0000005u) == "access violation");
    CHECK(CrashReport::ExceptionName(0xC00000FDu) == "stack overflow");
    CHECK(CrashReport::ExceptionName(0xE06D7363u) == "unhandled C++ exception");
    CHECK(CrashReport::ExceptionName(0x12345678u) == "exception 0x12345678");
    CHECK(CrashReport::FileStem(2026, 10, 11, 9, 5, 3) == "crash-20261011-090503");
    CHECK(CrashReport::FileStem(2026, 10, 11, 9, 5, 3) < CrashReport::FileStem(2026, 10, 11, 10, 0, 0));
}

TEST_CASE("Memory reads as one line, and says what couldn't be measured")
{
    constexpr std::uint64_t MB = 1024 * 1024;
    CHECK(Memory::Megabytes(0) == "0.0 MB");
    CHECK(Memory::Megabytes(MB * 3 / 2) == "1.5 MB");
    CHECK(Memory::Describe(Memory::ProcessMemory{ 300 * MB, 340 * MB, 280 * MB },
                           Memory::GpuMemory{ 210 * MB, 3800 * MB, true })
          == "RAM 300.0 MB (peak 340.0 MB)   GPU 210.0 MB of 3800.0 MB (shared)");
    CHECK(Memory::Describe(std::nullopt, std::nullopt) == "RAM unavailable   GPU unavailable");
}

TEST_CASE("The process's own memory can be read")
{
#ifdef _WIN32
    const auto process = Memory::QueryProcess();
    REQUIRE(process.has_value());
    CHECK(process->workingSetBytes > 0);
    CHECK(process->peakWorkingSetBytes >= process->workingSetBytes);
    CHECK_FALSE(Memory::QueryGpu("no such adapter").has_value());
#endif
}
