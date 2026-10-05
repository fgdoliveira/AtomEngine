#pragma once

#include <SDL3/SDL_gpu.h>

#include <cstdint>
#include <optional>
#include <string>

namespace Atom
{
    enum class GPUPreference
    {
        LowPower,
        HighPerformance
    };

    const char* GetPreferenceName(GPUPreference preference);

    struct GPUDeviceConfig
    {
        GPUPreference preference = GPUPreference::LowPower;
        bool debugMode = false;
    };

    // How the window's images reach the screen. Device and window concerns,
    // not scene rendering.
    struct PresentationConfig
    {
        bool vsync = true;
        bool preferImmediate = false; // ATOM_PRESENT=immediate (M46)
        std::uint32_t framesInFlight = 2; // M74: measured lower latency (see GPUDevice.cpp)
    };

    // Why the requested adapter isn't the one in use. Two different steps
    // can fail: creating the device, or presenting it to this window (the
    // hybrid laptop's RTX is created fine; its swapchain for the built-in
    // panel is refused).
    struct StartupFallback
    {
        enum class Stage { Device, Presentation };
        std::string adapter; // empty when no device was created
        Stage stage = Stage::Device;
        std::string error;   // SDL's text, verbatim
    };

    struct GPUDeviceInfo
    {
        std::string backend;
        std::string adapter;
        GPUPreference requestedPreference = GPUPreference::LowPower;
        GPUPreference activePreference = GPUPreference::LowPower;
        std::optional<StartupFallback> fallback;

        std::string composition; // "sdr-linear" | "sdr"
        std::string presentMode; // "vsync" | "mailbox" | "immediate"
        bool supportsVsync = false;
        bool supportsMailbox = false;
        bool supportsImmediate = false;
        std::uint32_t framesInFlight = 2; // SDL's default until configured
    };

    enum class PresentMode { Vsync, Mailbox, Immediate };

    // Pure: the present mode for a config, given what the window supports.
    // Without vsync, tear-free mailbox first, else immediate; immediate first
    // when asked (a composited display can hold mailbox to its refresh rate).
    PresentMode ChoosePresentMode(const PresentationConfig& config, bool supportsMailbox, bool supportsImmediate);
    const char* GetPresentModeName(PresentMode mode);

    // Pure (M74): with vsync and 2 frames in flight, does the frame interval
    // show that 2 can't hold the display's refresh (median > 1.6 refresh
    // periods - a halved rate, or a GPU too slow for it)? Then 3 is needed.
    bool NeedsThirdFrame(double medianIntervalMs, double refreshHz);

    // Pure: the requested preference and why it wasn't honoured, e.g.
    // `high_performance (fell back: "NVIDIA ..." could not present to this window)`.
    std::string DescribePreference(GPUPreference requested, const std::optional<StartupFallback>& fallback);
    // Pure: the reason alone, e.g. `"NVIDIA ..." could not present to this window`.
    std::string DescribeFallback(const StartupFallback& fallback);

    // The SDL GPU device and the window it presents to: adapter choice and
    // the hybrid-laptop fallback, the window claim, and the swapchain's
    // composition, present mode and frames in flight. Owned by the Renderer;
    // SDL is the backend abstraction (ADR-001), so there is no interface.
    class GPUDevice
    {
    public:
        GPUDevice() = default;
        ~GPUDevice();

        GPUDevice(const GPUDevice&) = delete;
        GPUDevice& operator=(const GPUDevice&) = delete;

        // Creates the device and claims the window. A high-performance
        // request that fails at either step retries low-power (M63) and
        // records why in GetInfo().fallback.
        bool Initialize(SDL_Window* window, const GPUDeviceConfig& config);
        bool ConfigurePresentation(const PresentationConfig& config);
        // M64: present without waiting for the display while measuring,
        // then back to the configured mode. False if the swapchain refused.
        bool SetUncappedPresentation(bool uncapped);
        // M74: block until the swapchain can take the next frame, so the
        // frame's acquire won't (SDL_WaitForGPUSwapchain; in SDL 3.4.18's
        // D3D12 backend, the fence of the next swapchain slot - the same
        // one the acquire waits on).
        bool WaitForPresentSlot();
        // M74: change frames in flight while running (SDL waits for the GPU
        // and rebuilds the swapchain: a one-off stall).
        bool SetFramesInFlight(std::uint32_t frames);
        // abandon: leave the device to the OS (see the .cpp, M63).
        void Shutdown(bool abandon = false);

        SDL_GPUDevice* Get() const { return m_device; }
        SDL_Window* GetWindow() const { return m_window; }
        bool IsWindowClaimed() const { return m_windowClaimed; }
        const GPUDeviceInfo& GetInfo() const { return m_info; }

    private:
        bool CreateAndClaim(GPUPreference preference, bool debugMode);

        SDL_GPUDevice* m_device = nullptr;
        SDL_Window* m_window = nullptr;
        bool m_windowClaimed = false;
        SDL_GPUSwapchainComposition m_composition = SDL_GPU_SWAPCHAINCOMPOSITION_SDR_LINEAR;
        SDL_GPUPresentMode m_configuredMode = SDL_GPU_PRESENTMODE_VSYNC;
        GPUDeviceInfo m_info;
    };
}
