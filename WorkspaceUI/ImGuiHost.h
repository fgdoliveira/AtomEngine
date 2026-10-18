#pragma once

#include <string>

union SDL_Event;
struct SDL_GPUCommandBuffer;
struct SDL_GPUDevice;
struct SDL_GPUTexture;
struct SDL_Window;

namespace AtomWorkspace
{
    // The workspace's Dear ImGui (v0.0.19; the engine's F10 host until
    // then, M41): the docking branch, so panels dock into one window or
    // detach into windows of their own (multi-viewport, the SDL_GPU
    // backend: one device, a swapchain per window). Immediate mode: each
    // frame the workspace calls ImGui; ImGui keeps the layout.
    class ImGuiHost
    {
    public:
        struct Options
        {
            std::string layoutFile;  // where panel placement is kept ("" = not kept)
            bool viewports = true;   // panels may leave the main window
        };

        bool Initialize(SDL_Window* window, SDL_GPUDevice* device, const Options& options);
        void Shutdown();
        bool IsInitialized() const { return m_initialized; }

        // Every window event, before input sees it. True when ImGui wants
        // it (the mouse over a panel, the keyboard in a field).
        bool HandleEvent(const SDL_Event& event);

        void BeginFrame();
        // The renderer's overlay pass: the main window's panels onto
        // `target`, then the detached windows (their own swapchains).
        void Render(SDL_GPUCommandBuffer* commandBuffer, SDL_GPUTexture* target);

        // Forgets the saved placement; the workspace lays the panels out
        // again from its default on the next frame.
        void ResetLayout();
        bool TakeLayoutReset(); // true once after ResetLayout (or with no saved layout)
        bool ViewportsEnabled() const { return m_viewports; }

    private:
        bool m_initialized = false;
        bool m_frameActive = false;
        bool m_viewports = false;
        bool m_layoutReset = false;
        std::string m_layoutFile;
        SDL_GPUDevice* m_device = nullptr;
    };
}
