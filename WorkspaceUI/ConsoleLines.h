#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace AtomWorkspace
{
    // What the Console makes of a line of a tool's output (v0.0.19, M125):
    // how bad it is, and the file it names - so a failure leads to the
    // file first, then the message.
    enum class LineKind
    {
        Plain,
        Note,    // the workspace's own lines ("> Build: ...", exit status)
        Success, // "PASS", "ok", "Build succeeded"
        Warning,
        Error,
    };

    LineKind ClassifyLine(std::string_view line);

    // The file a line names, if it exists, found from `searchRoots` when
    // relative: MSVC's "<file>(<line>[,<col>]): error ...", CMake's
    // "CMake Error at <file>:<line>", AtomLevelCheck's "  Source: <file>..."
    // ("" when the line names none, or one that isn't there).
    std::string FileInLine(std::string_view line, const std::vector<std::string>& searchRoots);
}
