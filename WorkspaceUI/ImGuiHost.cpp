#include "ImGuiHost.h"
#include "Core/Log.h"

#include <SDL3/SDL.h>

#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlgpu3.h>

#include <filesystem>

namespace AtomWorkspace
{
    namespace
    {
        // One look for the engine's tools (v0.0.14, M89; the workspace's
        // since v0.0.19): ImGui's dark theme, a little more air, and the
        // engine's warm accent in place of ImGui's blue - the games' too.
        void ApplyStyle(bool viewports)
        {
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
            colors[ImGuiCol_TabSelected] = { 0.30f, 0.21f, 0.13f, 1.0f };
            colors[ImGuiCol_DockingPreview] = accentDim;
            // A detached panel is an OS window: square, opaque, so it looks
            // the same docked or not (ImGui's own advice).
            if (viewports)
            {
                style.WindowRounding = 0.0f;
                colors[ImGuiCol_WindowBg].w = 1.0f;
            }
        }
    }

    bool ImGuiHost::Initialize(SDL_Window* window, SDL_GPUDevice* device, const Options& options)
    {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_DockingEnable;
        m_viewports = options.viewports;
        if (m_viewports)
        {
            io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
            io.ConfigDpiScaleFonts = true;     // a panel moved to another monitor follows its DPI
            io.ConfigDpiScaleViewports = true;
        }
        // The layout lives in the user's settings, never beside the
        // executable; no file yet means the default layout.
        m_layoutFile = options.layoutFile;
        io.IniFilename = m_layoutFile.empty() ? nullptr : m_layoutFile.c_str();
        std::error_code error;
        m_layoutReset = m_layoutFile.empty() || !std::filesystem::exists(m_layoutFile, error);

        ApplyStyle(m_viewports);
        // Sized for the main window's display (a 150 % laptop screen);
        // detached panels follow their own monitor (the flags above).
        if (const float scale = SDL_GetWindowDisplayScale(window); scale > 0.0f)
        {
            ImGui::GetStyle().ScaleAllSizes(scale);
            ImGui::GetStyle().FontScaleDpi = scale;
        }
        m_device = device;

        ImGui_ImplSDLGPU3_InitInfo info{};
        info.Device = device;
        info.ColorTargetFormat = SDL_GetGPUSwapchainTextureFormat(device, window);
        info.MSAASamples = SDL_GPU_SAMPLECOUNT_1;
        // The detached windows' swapchains must match the main one's: ImGui
        // has one pipeline, built for ColorTargetFormat. The renderer picks
        // SDR_LINEAR (an *_SRGB format) where the window supports it, plain
        // SDR otherwise (GPUDevice.cpp) - a mismatch is D3D12 error #613.
        const SDL_GPUTextureFormat format = info.ColorTargetFormat;
        const bool srgb = format == SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB || format == SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB;
        info.SwapchainComposition = srgb ? SDL_GPU_SWAPCHAINCOMPOSITION_SDR_LINEAR : SDL_GPU_SWAPCHAINCOMPOSITION_SDR;
        info.PresentMode = SDL_GPU_PRESENTMODE_VSYNC;
        if (!ImGui_ImplSDL3_InitForSDLGPU(window) || !ImGui_ImplSDLGPU3_Init(&info))
        {
            ATOM_LOG_ERROR("Workspace") << "Failed to initialise Dear ImGui.\n";
            ImGui::DestroyContext();
            return false;
        }
        m_initialized = true;
        return true;
    }

    void ImGuiHost::Shutdown()
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
        SDL_WaitForGPUIdle(m_device); // its vertex buffers may still be in flight
        ImGui_ImplSDLGPU3_Shutdown();
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext(); // writes the layout file
        m_initialized = false;
    }

    bool ImGuiHost::HandleEvent(const SDL_Event& event)
    {
        if (!m_initialized)
        {
            return false;
        }
        ImGui_ImplSDL3_ProcessEvent(&event);
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

    void ImGuiHost::BeginFrame()
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
            if (m_viewports)
            {
                ImGui::UpdatePlatformWindows();
            }
            m_frameActive = false;
        }
        ImGui_ImplSDLGPU3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();
        m_frameActive = true;
    }

    void ImGuiHost::Render(SDL_GPUCommandBuffer* commandBuffer, SDL_GPUTexture* target)
    {
        if (!m_frameActive)
        {
            return;
        }
        m_frameActive = false;
        ImGui::Render();
        ImDrawData* drawData = ImGui::GetDrawData();
        if (drawData && drawData->DisplaySize.x > 0.0f && drawData->DisplaySize.y > 0.0f)
        {
            // Vertices first, in a copy pass (SDL_GPU's rule), then the draw
            // over what the window already holds.
            ImGui_ImplSDLGPU3_PrepareDrawData(drawData, commandBuffer);
            SDL_GPUColorTargetInfo colorTarget{};
            colorTarget.texture = target;
            colorTarget.load_op = SDL_GPU_LOADOP_LOAD;
            colorTarget.store_op = SDL_GPU_STOREOP_STORE;
            if (SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(commandBuffer, &colorTarget, 1, nullptr))
            {
                ImGui_ImplSDLGPU3_RenderDrawData(drawData, commandBuffer, pass);
                SDL_EndGPURenderPass(pass);
            }
        }
        // The detached panels: each its own window and swapchain, drawn
        // with command buffers of their own, before the main one submits.
        if (m_viewports)
        {
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
        }
    }

    void ImGuiHost::ResetLayout()
    {
        ImGui::LoadIniSettingsFromMemory("");
        if (!m_layoutFile.empty())
        {
            std::error_code error;
            std::filesystem::remove(m_layoutFile, error);
        }
        m_layoutReset = true;
    }

    bool ImGuiHost::TakeLayoutReset()
    {
        const bool reset = m_layoutReset;
        m_layoutReset = false;
        return reset;
    }
}
