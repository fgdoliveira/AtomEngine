#include "ConsoleLines.h"
#include "ProcessRunner.h"

#include <SDL3/SDL.h>
#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <vector>

using namespace AtomWorkspace;
namespace fs = std::filesystem;

namespace
{
    // Polls as the workspace does, each "frame", until the process ends.
    struct Finished
    {
        std::vector<std::string> lines;
        std::optional<int> exitCode;
    };

    Finished RunToEnd(ProcessRunner& runner, double timeoutSeconds = 20.0)
    {
        Finished result;
        const std::uint64_t start = SDL_GetTicks();
        while (!result.exitCode && SDL_GetTicks() - start < static_cast<std::uint64_t>(timeoutSeconds * 1000.0))
        {
            for (std::string& line : runner.TakeLines())
            {
                result.lines.push_back(std::move(line));
            }
            result.exitCode = runner.TakeExitCode();
            SDL_Delay(5);
        }
        return result;
    }
}

TEST_CASE("Operations: a process's output arrives in lines, then its exit code")
{
    ProcessRunner runner;
    REQUIRE(runner.Start({ ATOM_TEST_CMAKE, "-E", "echo", "hello from a child" }));
    CHECK(runner.IsRunning() == true);
    CHECK_FALSE(runner.Start({ ATOM_TEST_CMAKE, "-E", "echo", "second" })); // one at a time
    const Finished finished = RunToEnd(runner);
    REQUIRE(finished.exitCode.has_value());
    CHECK(*finished.exitCode == 0);
    REQUIRE(finished.lines.size() == 1);
    CHECK(finished.lines[0] == "hello from a child");
    CHECK_FALSE(runner.IsRunning());
    CHECK_FALSE(runner.TakeExitCode().has_value()); // once
}

TEST_CASE("Operations: a failing process reports its exit code; a missing one doesn't start")
{
    ProcessRunner runner;
    REQUIRE(runner.Start({ ATOM_TEST_CMAKE, "-E", "false" }));
    const Finished finished = RunToEnd(runner);
    REQUIRE(finished.exitCode.has_value());
    CHECK(*finished.exitCode != 0);

    CHECK_FALSE(runner.Start({ "D:/no such folder/no_such_tool.exe" }));
    CHECK_FALSE(runner.Error().empty());
}

TEST_CASE("Operations: Stop ends a long process without waiting for it")
{
    ProcessRunner runner;
    REQUIRE(runner.Start({ ATOM_TEST_CMAKE, "-E", "sleep", "30" }));
    SDL_Delay(200);
    runner.Stop();
    const Finished finished = RunToEnd(runner, 5.0);
    REQUIRE(finished.exitCode.has_value());
    CHECK(runner.WasStopped());
    CHECK(runner.Seconds() < 5.0);
}

TEST_CASE("Console: lines are classified")
{
    CHECK(ClassifyLine("> Build: cmake --build build") == LineKind::Note);
    CHECK(ClassifyLine("PASS: 0 error(s)") == LineKind::Success);
    CHECK(ClassifyLine("ok   demo/shore: 0 error(s), 0 warning(s)") == LineKind::Success);
    CHECK(ClassifyLine("FAIL demo/shore: 2 error(s), 0 warning(s)") == LineKind::Error);
    CHECK(ClassifyLine("ERROR [shore]") == LineKind::Error);
    CHECK(ClassifyLine("D:\\Src\\Main.cpp(12,5): error C2065: 'x': undeclared identifier") == LineKind::Error);
    CHECK(ClassifyLine("D:\\Src\\Main.cpp(3): warning C4100: unreferenced parameter") == LineKind::Warning);
    CHECK(ClassifyLine("CMake Error at Apps/Sandbox/CMakeLists.txt:4 (add_executable):") == LineKind::Error);
    CHECK(ClassifyLine("    0 Error(s)") == LineKind::Success);
    CHECK(ClassifyLine("  Building Custom Rule") == LineKind::Plain);
}

TEST_CASE("Console: the file a line names, when it exists")
{
    const fs::path root = fs::temp_directory_path() / "atom console tests" / "my project";
    fs::create_directories(root / "Assets" / "Levels");
    fs::create_directories(root / "Source");
    std::ofstream(root / "Assets" / "Levels" / "street.json") << "{}";
    std::ofstream(root / "Source" / "Main.cpp") << "int main() {}";
    const std::vector<std::string> roots{ (root / "Assets").string(), root.string() };

    // AtomLevelCheck: relative to the asset root, maybe with a pointer.
    CHECK(fs::path(FileInLine("  Source: Levels/street.json", roots)) == (root / "Assets" / "Levels" / "street.json"));
    CHECK(fs::path(FileInLine("  Source: Levels/street.json#/entities/3", roots))
          == (root / "Assets" / "Levels" / "street.json"));
    // MSVC, absolute, a space in the path.
    const std::string main = (root / "Source" / "Main.cpp").string();
    CHECK(fs::path(FileInLine(main + "(12,5): error C2065: 'x': undeclared identifier", roots)) == root / "Source" / "Main.cpp");
    // CMake, relative to the project.
    CHECK(fs::path(FileInLine("CMake Error at Source/Main.cpp:4 (add_executable):", roots)) == root / "Source" / "Main.cpp");
    // Nothing named, or a file that isn't there.
    CHECK(FileInLine("Build succeeded.", roots).empty());
    CHECK(FileInLine("  Source: Levels/missing.json", roots).empty());
    CHECK(FileInLine("call(1): not a file", roots).empty());

    std::error_code ignored;
    fs::remove_all(root.parent_path(), ignored);
}
