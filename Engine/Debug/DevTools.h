#pragma once

union SDL_Event;
struct SDL_GPUCommandBuffer;
struct SDL_GPUDevice;
struct SDL_GPUTexture;
struct SDL_Window;

namespace Atom
{
    // Developer tools (M41): Dear ImGui panels for looking inside the engine
    // and tuning it live. F10 shows and hides them. They are a tool, not part
    // of the game: drawn last, straight onto the window, never into
    // screenshots or captures, and they take the mouse and keyboard only
    // while shown and in use.
    //
    // Immediate mode: panels are not objects that persist. Each frame the
    // game calls ImGui functions (ImGui::Begin, ImGui::SliderFloat...) that
    // both draw a widget and report what the user did with it; ImGui keeps
    // only the little state it needs between frames (window positions,
    // which widget is active).
    class DevTools
    {
    public:
        bool Initialize(SDL_Window* window, SDL_GPUDevice* device);
        void Shutdown();

        // Every window event, before the game sees it. Returns true when
        // the tools used it (F10, or typing and clicking in a panel), so the
        // game should ignore it.
        bool HandleEvent(const SDL_Event& event);

        // Starts a frame of panels when shown. Between BeginFrame and the
        // renderer's overlay pass, the game may call ImGui.
        void BeginFrame();
        bool IsFrameActive() const { return m_frameActive; }

        // The renderer's overlay pass: draws this frame's panels onto
        // `target` (the swapchain image), over everything else.
        void Render(SDL_GPUCommandBuffer* commandBuffer, SDL_GPUTexture* target);

        bool IsVisible() const { return m_visible; }
        void SetVisible(bool visible) { m_visible = visible; }
        bool IsInitialized() const { return m_initialized; }

    private:
        bool m_initialized = false;
        bool m_visible = false;
        bool m_frameActive = false;
    };
}
