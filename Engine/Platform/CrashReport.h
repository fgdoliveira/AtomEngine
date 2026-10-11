#pragma once

#include <cstdint>
#include <string>

namespace Atom::CrashReport
{
    // When the program crashes (an unhandled exception: an access
    // violation, a stack overflow...), write what's needed to find out why
    // (v0.0.15, M101), into `folder`:
    //   crash-<date>-<time>.dmp   a minidump: the threads, their stacks and
    //                             the memory they point at - opened in Visual
    //                             Studio or WinDbg with the build's .pdb
    //                             (kept by the developer, never shipped)
    //   crash-<date>-<time>.txt   the app, version, exception and module
    // and say where on std::cerr (so the log ends with it). The report holds
    // no paths but the module's name. Windows only; elsewhere, nothing.
    void Install(const std::string& folder, const std::string& app); // the version is the engine's
    bool IsInstalled();

    // Pure (unit-tested): the readable name of a Windows exception code.
    std::string ExceptionName(std::uint32_t code);
    // Pure: the files' common stem, e.g. "crash-20261011-153012".
    std::string FileStem(int year, int month, int day, int hour, int minute, int second);
}
