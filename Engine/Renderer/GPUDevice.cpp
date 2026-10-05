#include "Renderer/GPUDevice.h"

#include <SDL3/SDL.h>

#include <iostream>

namespace Atom
{
    namespace
    {
        SDL_GPUPresentMode ToSDL(PresentMode mode)
        {
            switch (mode)
            {
            case PresentMode::Mailbox:
                return SDL_GPU_PRESENTMODE_MAILBOX;
            case PresentMode::Immediate:
                return SDL_GPU_PRESENTMODE_IMMEDIATE;
            case PresentMode::Vsync:
                break;
            }
            return SDL_GPU_PRESENTMODE_VSYNC;
        }

        const char* GetModeName(SDL_GPUPresentMode mode)
        {
            return mode == SDL_GPU_PRESENTMODE_IMMEDIATE ? "immediate"
                : mode == SDL_GPU_PRESENTMODE_MAILBOX ? "mailbox" : "vsync";
        }

        // Every way to get a device failed. Seen on the hybrid laptop when
        // Windows' per-app Graphics setting forced one GPU for AtomGame.exe:
        // Windows then hands that GPU out for *both* preferences, so the
        // low-power fallback can't reach the adapter that drives the screen.
        void ExplainNoDevice()
        {
            std::cerr << "No GPU could present to this window. On a laptop with two GPUs, check Windows Settings > "
                         "System > Display > Graphics: a GPU forced for AtomGame.exe applies to every request; "
                         "'Let Windows decide' lets AtomEngine pick the GPU that drives this screen.\n";
        }
    }

    const char* GetPreferenceName(GPUPreference preference)
    {
        return preference == GPUPreference::LowPower ? "low_power" : "high_performance";
    }

    PresentMode ChoosePresentMode(const PresentationConfig& config, bool supportsMailbox, bool supportsImmediate)
    {
        if (config.vsync)
        {
            return PresentMode::Vsync;
        }
        // M46: MAILBOX is tear-free, but a composited display (an external
        // monitor, here) can still hold it to its refresh rate, which makes
        // frame times unmeasurable - so ATOM_PRESENT=immediate goes first.
        if (config.preferImmediate)
        {
            return supportsImmediate ? PresentMode::Immediate
                : supportsMailbox    ? PresentMode::Mailbox
                                     : PresentMode::Vsync;
        }
        return supportsMailbox ? PresentMode::Mailbox
            : supportsImmediate ? PresentMode::Immediate
                                : PresentMode::Vsync;
    }

    const char* GetPresentModeName(PresentMode mode)
    {
        return GetModeName(ToSDL(mode));
    }

    std::string DescribeFallback(const StartupFallback& fallback)
    {
        const std::string who = fallback.adapter.empty() ? std::string("the device")
                                                         : '"' + fallback.adapter + '"';
        return fallback.stage == StartupFallback::Stage::Presentation
            ? who + " could not present to this window"
            : who + " could not be created";
    }

    std::string DescribePreference(GPUPreference requested, const std::optional<StartupFallback>& fallback)
    {
        std::string text = GetPreferenceName(requested);
        if (fallback)
        {
            text += " (fell back: " + DescribeFallback(*fallback) + ")";
        }
        return text;
    }

    GPUDevice::~GPUDevice()
    {
        Shutdown();
    }

    bool GPUDevice::CreateAndClaim(GPUPreference preference, bool debugMode)
    {
        const SDL_PropertiesID properties = SDL_CreateProperties();
        if (!properties)
        {
            std::cerr << "Failed to create GPU device properties: " << SDL_GetError() << '\n';
            return false;
        }
        const bool propertiesConfigured =
            SDL_SetStringProperty(properties, SDL_PROP_GPU_DEVICE_CREATE_NAME_STRING, "direct3d12")
            && SDL_SetBooleanProperty(properties, SDL_PROP_GPU_DEVICE_CREATE_SHADERS_DXIL_BOOLEAN, true)
            && SDL_SetBooleanProperty(properties, SDL_PROP_GPU_DEVICE_CREATE_DEBUGMODE_BOOLEAN, debugMode)
            && SDL_SetBooleanProperty(properties, SDL_PROP_GPU_DEVICE_CREATE_PREFERLOWPOWER_BOOLEAN,
                                      preference == GPUPreference::LowPower);
        if (!propertiesConfigured)
        {
            const std::string error = SDL_GetError();
            SDL_DestroyProperties(properties);
            std::cerr << "Failed to configure GPU device properties: " << error << '\n';
            return false;
        }

        m_device = SDL_CreateGPUDeviceWithProperties(properties);
        const std::string creationError = m_device ? "" : SDL_GetError();
        SDL_DestroyProperties(properties);
        if (!m_device)
        {
            std::cerr << "GPU device creation failed (preference=" << GetPreferenceName(preference)
                      << "): " << creationError << '\n';
            m_info.fallback = StartupFallback{ "", StartupFallback::Stage::Device, creationError };
            return false;
        }

        // The adapter is known now, before the window claim - logged first,
        // so a claim that fails still says which GPU it failed on.
        const char* backend = SDL_GetGPUDeviceDriver(m_device);
        const char* adapter = SDL_GetStringProperty(
            SDL_GetGPUDeviceProperties(m_device), SDL_PROP_GPU_DEVICE_NAME_STRING, "unavailable");
        m_info.backend = backend ? backend : "unavailable";
        m_info.adapter = adapter;
        std::cout << "GPU device created: backend=" << m_info.backend << " adapter=\"" << m_info.adapter << '"'
                  << " preference=" << GetPreferenceName(preference) << '\n';

        // Claiming creates the window's swapchain: a separate step that can
        // fail on its own (the hybrid laptop's RTX, for the built-in panel).
        if (!SDL_ClaimWindowForGPUDevice(m_device, m_window))
        {
            const std::string error = SDL_GetError();
            std::cerr << "GPU presentation failed on \"" << m_info.adapter
                      << "\": could not claim the window: " << error << '\n';
            m_info.fallback = StartupFallback{ m_info.adapter, StartupFallback::Stage::Presentation, error };
            SDL_DestroyGPUDevice(m_device);
            m_device = nullptr;
            return false;
        }
        m_windowClaimed = true;
        std::cout << "GPU presentation: window claimed on \"" << m_info.adapter << "\"\n";
        return true;
    }

    bool GPUDevice::Initialize(SDL_Window* window, const GPUDeviceConfig& config)
    {
        Shutdown();
        if (!window)
        {
            std::cerr << "Cannot create a GPU device for a null window.\n";
            return false;
        }
        m_window = window;
        m_info = GPUDeviceInfo{};
        m_info.requestedPreference = config.preference;
        m_info.activePreference = config.preference;

        if (CreateAndClaim(config.preference, config.debugMode))
        {
            m_info.fallback.reset(); // a failed step before success is history
            return true;
        }
        // M63: high-performance is only a preference; if that device can't
        // be made or can't present, the low-power one is the known-good
        // choice. Nothing has been created on the failed device yet.
        if (config.preference != GPUPreference::HighPerformance)
        {
            ExplainNoDevice();
            return false;
        }
        const StartupFallback why = *m_info.fallback;
        std::cerr << "High-performance: " << DescribeFallback(why) << "; falling back to low-power.\n";
        m_info.activePreference = GPUPreference::LowPower;
        if (!CreateAndClaim(GPUPreference::LowPower, config.debugMode))
        {
            ExplainNoDevice();
            return false;
        }
        m_info.fallback = why; // the reason for the fallback, not the retry
        return true;
    }

    bool GPUDevice::ConfigurePresentation(const PresentationConfig& config)
    {
        if (!m_device || !m_windowClaimed)
        {
            return false;
        }
        // Shaders work in linear space (textures are sampled as sRGB), so
        // let the swapchain do the linear -> sRGB encode on write.
        SDL_GPUSwapchainComposition composition = SDL_GPU_SWAPCHAINCOMPOSITION_SDR_LINEAR;
        if (!SDL_WindowSupportsGPUSwapchainComposition(m_device, m_window, composition))
        {
            std::cerr << "sRGB swapchain unsupported; colors will look dark.\n";
            composition = SDL_GPU_SWAPCHAINCOMPOSITION_SDR;
        }

        // For diagnostics (M62): what the window offers.
        m_info.supportsVsync = SDL_WindowSupportsGPUPresentMode(m_device, m_window, SDL_GPU_PRESENTMODE_VSYNC);
        m_info.supportsMailbox = SDL_WindowSupportsGPUPresentMode(m_device, m_window, SDL_GPU_PRESENTMODE_MAILBOX);
        m_info.supportsImmediate = SDL_WindowSupportsGPUPresentMode(m_device, m_window, SDL_GPU_PRESENTMODE_IMMEDIATE);
        const SDL_GPUPresentMode mode = ToSDL(ChoosePresentMode(config, m_info.supportsMailbox, m_info.supportsImmediate));

        if (!SDL_SetGPUSwapchainParameters(m_device, m_window, composition, mode))
        {
            std::cerr << "Failed to set swapchain parameters: " << SDL_GetError() << '\n';
            return false;
        }
        m_composition = composition;
        m_configuredMode = mode;
        m_info.composition = composition == SDL_GPU_SWAPCHAINCOMPOSITION_SDR_LINEAR ? "sdr-linear" : "sdr";
        m_info.presentMode = GetModeName(mode);

        // Frames the CPU may record ahead of the GPU. In SDL's D3D12 backend
        // this is also the swapchain's buffer count (clamped to 2..3). SDL's
        // default 2 is double buffering: with vsync the game waits for a
        // refresh to free a buffer and misses every other one - 72 fps on a
        // 144 Hz panel with 3.4 ms frames (paired runs: 13.9 ms vs 6.95 ms
        // with 3). 3 also keeps a clock-dropping iGPU busy (the Iris Xe swung
        // 3.5 <-> 9.7 ms with 2). A deeper queue can add a frame of input
        // delay, but at twice the frame rate each frame is half as long.
        if (SDL_SetGPUAllowedFramesInFlight(m_device, config.framesInFlight))
        {
            m_info.framesInFlight = config.framesInFlight;
        }
        else
        {
            std::cerr << "Frames in flight " << config.framesInFlight << " refused: " << SDL_GetError() << '\n';
        }
        return true;
    }

    bool GPUDevice::SetUncappedPresentation(bool uncapped)
    {
        if (!m_device || !m_windowClaimed)
        {
            return false;
        }
        SDL_GPUPresentMode mode = m_configuredMode;
        if (uncapped)
        {
            // Immediate first: on a composited display mailbox can still be
            // held to the refresh rate (M46), and then nothing is measured.
            const PresentationConfig measuring{ false, true, m_info.framesInFlight };
            mode = ToSDL(ChoosePresentMode(measuring, m_info.supportsMailbox, m_info.supportsImmediate));
        }
        if (!SDL_SetGPUSwapchainParameters(m_device, m_window, m_composition, mode))
        {
            std::cerr << "Could not change the present mode: " << SDL_GetError() << '\n';
            return false;
        }
        m_info.presentMode = GetModeName(mode);
        return true;
    }

    void GPUDevice::Shutdown(bool abandon)
    {
        if (m_device)
        {
            if (abandon)
            {
                // M63: after a D3D12 swapchain failed to resize its buffers
                // (the hybrid-laptop crossing), SDL can neither release the
                // window from the device nor destroy the device - both free
                // the broken swapchain and corrupt the heap (0xC0000374,
                // reproduced on the RTX 4060). The renderer has released
                // everything else; the device and its swapchain are
                // abandoned and the process, which is exiting, returns them
                // to Windows.
                std::cerr << "Leaving the failed GPU device to the operating system.\n";
            }
            else
            {
                if (m_windowClaimed && m_window)
                {
                    SDL_ReleaseWindowFromGPUDevice(m_device, m_window);
                }
                SDL_DestroyGPUDevice(m_device);
            }
        }
        m_device = nullptr;
        m_window = nullptr;
        m_windowClaimed = false;
    }
}
