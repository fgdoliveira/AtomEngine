#pragma once

#include <string>

struct SDL_Window;

namespace Atom
{
    class Renderer;
}

namespace AtomFramework
{
    // The power source in words ("plugged in", "battery (64%)"): context for
    // any timing a run reports.
    std::string PowerStateName();

    // --diagnostics (M62, shared since M82): the machine's facts, one per
    // line, for doctor.ps1 and bug reports - SDL, the adapter and any
    // fallback, presentation, display and power. `title` is the first line;
    // the game appends its own lines (quality, settings) after these.
    std::string DiagnosticsReport(const std::string& title, Atom::Renderer& renderer, SDL_Window* window,
                                  const std::string& gpuReason);

    // Writes the report and says so on stdout ("Diagnostics written to ...",
    // what the scenarios look for). False when the file can't be written.
    bool WriteDiagnosticsFile(const std::string& path, const std::string& text);
}
