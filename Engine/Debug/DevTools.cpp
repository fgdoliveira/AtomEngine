#include "Debug/DevTools.h"

#include <SDL3/SDL.h>

#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlgpu3.h>

#include <iostream>

namespace Atom
{
    bool DevTools::Initialize(SDL_Window* window, SDL_GPUDevice* device)
    {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        // No imgui.ini next to the executable: panels open where they're
        // placed in code each run.
        io.IniFilename = nullptr;
        // One look for every app's tools (v0.0.14, M89): ImGui's dark theme,
        // a little more air and rounding, and the engine's warm accent in
        // place of ImGui's blue - the same as the games' UI.
        ImGui::StyleColorsDark();
        ImGuiStyle& style = ImGui::GetStyle();
        style.WindowPadding = { 10.0f, 10.0f };
        style.FramePadding = { 6.0f, 4.0f };
        style.ItemSpacing = { 8.0f, 6.0f };
        style.WindowRounding = 6.0f;
        style.FrameRounding = 4.0f;
        style.GrabRounding = 4.0f;
        style.WindowBorderSize = 0.0f;
        ImVec4* colors = style.Colors;
        const ImVec4 accent{ 0.93f, 0.62f, 0.32f, 1.0f };
        const ImVec4 accentDim{ 0.93f, 0.62f, 0.32f, 0.55f };
        colors[ImGuiCol_WindowBg] = { 0.05f, 0.05f, 0.06f, 0.88f };
        colors[ImGuiCol_TitleBg] = { 0.09f, 0.08f, 0.08f, 1.0f };
        colors[ImGuiCol_TitleBgActive] = { 0.20f, 0.13f, 0.08f, 1.0f };
        colors[ImGuiCol_FrameBg] = { 0.16f, 0.15f, 0.15f, 1.0f };
        colors[ImGuiCol_FrameBgHovered] = { 0.26f, 0.21f, 0.17f, 1.0f };
        colors[ImGuiCol_FrameBgActive] = { 0.33f, 0.25f, 0.18f, 1.0f };
        colors[ImGuiCol_CheckMark] = accent;
        colors[ImGuiCol_SliderGrab] = accentDim;
        colors[ImGuiCol_SliderGrabActive] = accent;
        colors[ImGuiCol_Button] = { 0.30f, 0.21f, 0.13f, 1.0f };
        colors[ImGuiCol_ButtonHovered] = accentDim;
        colors[ImGuiCol_ButtonActive] = accent;
        colors[ImGuiCol_Header] = { 0.30f, 0.21f, 0.13f, 1.0f };
        colors[ImGuiCol_HeaderHovered] = accentDim;
        colors[ImGuiCol_HeaderActive] = accent;
        colors[ImGuiCol_PlotLines] = accent;
        colors[ImGuiCol_PlotHistogram] = accent;
        colors[ImGuiCol_TextSelectedBg] = accentDim;
        colors[ImGuiCol_SeparatorHovered] = accentDim;
        colors[ImGuiCol_SeparatorActive] = accent;
        colors[ImGuiCol_ResizeGrip] = { 0.93f, 0.62f, 0.32f, 0.2f };
        colors[ImGuiCol_ResizeGripHovered] = accentDim;
        colors[ImGuiCol_ResizeGripActive] = accent;
        colors[ImGuiCol_Tab] = { 0.20f, 0.14f, 0.10f, 1.0f };
        colors[ImGuiCol_TabHovered] = accentDim;

        ImGui_ImplSDLGPU3_InitInfo info{};
        info.Device = device;
        info.ColorTargetFormat = SDL_GetGPUSwapchainTextureFormat(device, window);
        info.MSAASamples = SDL_GPU_SAMPLECOUNT_1;
        if (!ImGui_ImplSDL3_InitForSDLGPU(window) || !ImGui_ImplSDLGPU3_Init(&info))
        {
            std::cerr << "Failed to initialise the developer tools (ImGui).\n";
            ImGui::DestroyContext();
            return false;
        }
        m_initialized = true;
        return true;
    }

    void DevTools::Shutdown()
    {
        if (!m_initialized)
        {
            return;
        }
        if (m_frameActive)
        {
            ImGui::EndFrame();
            m_frameActive = false;
        }
        ImGui_ImplSDLGPU3_Shutdown();
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext();
        m_initialized = false;
    }

    bool DevTools::HandleEvent(const SDL_Event& event)
    {
        if (!m_initialized)
        {
            return false;
        }
        if (event.type == SDL_EVENT_KEY_DOWN && event.key.scancode == SDL_SCANCODE_F10 && !event.key.repeat)
        {
            m_visible = !m_visible;
            return true;
        }
        if (event.type == SDL_EVENT_KEY_DOWN && event.key.scancode == SDL_SCANCODE_F1 && !event.key.repeat)
        {
            m_overlayVisible = !m_overlayVisible; // M82: the shared overlay
            return true;
        }
        if (!m_visible)
        {
            return false;
        }
        ImGui_ImplSDL3_ProcessEvent(&event);

        // ImGui says what it wants this frame: the mouse over a panel, the
        // keyboard while a field has focus. Everything else is the game's.
        const ImGuiIO& io = ImGui::GetIO();
        switch (event.type)
        {
        case SDL_EVENT_MOUSE_MOTION:
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        case SDL_EVENT_MOUSE_BUTTON_UP:
        case SDL_EVENT_MOUSE_WHEEL:
            return io.WantCaptureMouse;
        case SDL_EVENT_KEY_DOWN:
        case SDL_EVENT_KEY_UP:
        case SDL_EVENT_TEXT_INPUT:
            return io.WantCaptureKeyboard;
        default:
            return false;
        }
    }

    void DevTools::BeginFrame()
    {
        if (!m_initialized)
        {
            return;
        }
        if (m_frameActive)
        {
            // Last frame never reached the overlay pass (a minimised
            // window): close it before starting another.
            ImGui::EndFrame();
            m_frameActive = false;
        }
        m_overlayLines.clear();
        if (!m_visible && !m_overlayVisible)
        {
            return;
        }
        ImGui_ImplSDLGPU3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();
        m_frameActive = true;
    }

    void DevTools::DrawOverlay()
    {
        // A text panel in the top-left corner that never takes the mouse or
        // keyboard: a readout, not a window to use.
        ImGui::SetNextWindowPos({ 12.0f, 12.0f }, ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.85f);
        constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs
            | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings
            | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav;
        if (ImGui::Begin("##AtomOverlay", nullptr, flags))
        {
            for (const std::string& line : m_overlayLines)
            {
                ImGui::TextUnformatted(line.c_str());
            }
        }
        ImGui::End(); // always, even when Begin returned false
    }

    void DevTools::Render(SDL_GPUCommandBuffer* commandBuffer, SDL_GPUTexture* target)
    {
        if (!m_frameActive)
        {
            return;
        }
        m_frameActive = false;
        if (m_overlayVisible)
        {
            DrawOverlay();
        }
        ImGui::Render();
        ImDrawData* drawData = ImGui::GetDrawData();
        if (!drawData || drawData->DisplaySize.x <= 0.0f || drawData->DisplaySize.y <= 0.0f)
        {
            return;
        }

        // Vertices and indices are uploaded in a copy pass first; the draw
        // itself then loads what's in the window and draws on top.
        ImGui_ImplSDLGPU3_PrepareDrawData(drawData, commandBuffer);
        SDL_GPUColorTargetInfo colorTarget{};
        colorTarget.texture = target;
        colorTarget.load_op = SDL_GPU_LOADOP_LOAD;
        colorTarget.store_op = SDL_GPU_STOREOP_STORE;
        SDL_GPURenderPass* renderPass = SDL_BeginGPURenderPass(commandBuffer, &colorTarget, 1, nullptr);
        if (!renderPass)
        {
            return;
        }
        ImGui_ImplSDLGPU3_RenderDrawData(drawData, commandBuffer, renderPass);
        SDL_EndGPURenderPass(renderPass);
    }
}
