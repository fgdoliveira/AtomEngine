#pragma once

struct SDL_Window;
struct SDL_GPUDevice;

namespace Atom
{
    enum class GPUPreference
    {
        LowPower,
        HighPerformance
    };

    struct RendererConfig
    {
        GPUPreference gpuPreference = GPUPreference::LowPower;
    };

    class Renderer
    {
    public:
        Renderer() = default;
        ~Renderer();

        Renderer(const Renderer&) = delete;
        Renderer& operator=(const Renderer&) = delete;

        bool Initialize(
            SDL_Window* window,
            const RendererConfig& config = {}
        );
        bool Render();
        void Shutdown();

    private:
        bool CreateAndClaimGPUDevice(GPUPreference preference);

        SDL_GPUDevice* m_device = nullptr;
        SDL_Window* m_window = nullptr;
        bool m_windowClaimed = false;
    };
}
