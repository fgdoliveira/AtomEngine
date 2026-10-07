#include "Diagnostics/DiagnosticsReport.h"

#include "Renderer/Renderer.h"

#include <SDL3/SDL.h>

#include <fstream>
#include <iostream>
#include <sstream>

namespace AtomFramework
{
    std::string PowerStateName()
    {
        int seconds = -1;
        int percent = -1;
        switch (SDL_GetPowerInfo(&seconds, &percent))
        {
        case SDL_POWERSTATE_ON_BATTERY: return "battery (" + std::to_string(percent) + "%)";
        case SDL_POWERSTATE_CHARGING: return "plugged in, charging";
        case SDL_POWERSTATE_CHARGED: return "plugged in";
        case SDL_POWERSTATE_NO_BATTERY: return "no battery (mains)";
        default: return "unknown";
        }
    }

    std::string DiagnosticsReport(const std::string& title, Atom::Renderer& renderer, SDL_Window* window,
                                  const std::string& gpuReason)
    {
        // Facts, one per line. Nothing here is a performance claim: those
        // need paired measurements.
        const Atom::Renderer::DeviceReport r = renderer.GetDeviceReport();
        const int compiled = SDL_VERSION;
        const int runtime = SDL_GetVersion();
        const auto version = [](int v) {
            return std::to_string(SDL_VERSIONNUM_MAJOR(v)) + '.' + std::to_string(SDL_VERSIONNUM_MINOR(v)) + '.'
                + std::to_string(SDL_VERSIONNUM_MICRO(v));
        };
        const SDL_DisplayID display = SDL_GetDisplayForWindow(window);
        const SDL_DisplayMode* mode = SDL_GetCurrentDisplayMode(display);
        const char* displayName = SDL_GetDisplayName(display);
        int windowWidth = 0;
        int windowHeight = 0;
        SDL_GetWindowSizeInPixels(window, &windowWidth, &windowHeight);

        std::ostringstream out;
        out << title << "\n"
            << "sdl.compiled: " << version(compiled) << "\n"
            << "sdl.runtime: " << version(runtime) << "\n"
            << "gpu.adapter: " << r.adapter << "\n"
            << "gpu.backend: " << r.backend << "\n"
            << "gpu.preference.requested: " << r.preference << "\n"
            << "gpu.preference.reason: " << gpuReason << "\n";
        if (const auto& fallback = renderer.GetStartupFallback())
        {
            out << "gpu.fallback.adapter: " << (fallback->adapter.empty() ? "none created" : fallback->adapter) << "\n"
                << "gpu.fallback.stage: "
                << (fallback->stage == Atom::StartupFallback::Stage::Presentation ? "presentation" : "device") << "\n"
                << "gpu.fallback.error: " << fallback->error << "\n";
        }
        out
            << "gpu.max_msaa: " << r.maxMsaa << "x\n"
            << "gpu.scene_format: " << r.sceneFormat << "\n"
            << "swapchain.composition: " << r.composition << "\n"
            << "swapchain.present_mode: " << r.presentMode << "\n"
            << "swapchain.frames_in_flight: " << r.framesInFlight << "\n"
            << "swapchain.supports: vsync=" << (r.supportsVsync ? "yes" : "no")
            << " mailbox=" << (r.supportsMailbox ? "yes" : "no")
            << " immediate=" << (r.supportsImmediate ? "yes" : "no") << "\n"
            << "display.name: " << (displayName ? displayName : "unknown") << "\n"
            << "display.mode: " << (mode ? std::to_string(mode->w) + "x" + std::to_string(mode->h) + " @ "
                                               + std::to_string(static_cast<int>(mode->refresh_rate + 0.5f)) + " Hz"
                                         : std::string("unknown")) << "\n"
            << "display.scale: " << SDL_GetWindowDisplayScale(window) << "\n"
            << "window.pixels: " << windowWidth << "x" << windowHeight << "\n"
            << "power: " << PowerStateName() << "\n";
        return out.str();
    }

    bool WriteDiagnosticsFile(const std::string& path, const std::string& text)
    {
        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        file << text;
        if (!file)
        {
            std::cerr << "Could not write diagnostics to " << path << '\n';
            return false;
        }
        std::cout << "Diagnostics written to " << path << std::endl;
        return true;
    }
}
