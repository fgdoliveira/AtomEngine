#include "Renderer/Renderer.h"
#include "Renderer/D3D12PresentationDiagnostics.h"
#include <SDL3/SDL.h>
#include <iostream>
#include <sstream>

namespace Atom
{
    Renderer::Renderer()
        : m_presentationDiagnostics(
            std::make_unique<D3D12PresentationDiagnostics>()
        )
    {
    }

    bool Renderer::CreateAndClaimGPUDevice(const char* stage)
    {
#ifndef NDEBUG
        constexpr bool enableDebug = true;
#else
        constexpr bool enableDebug = false;
#endif

        const std::string stageName = stage;
        LogDiagnostic(
            stageName +
            "_device_create_started requested_backend=\"direct3d12\" debug=" +
            (enableDebug ? "true" : "false")
        );

        m_device = SDL_CreateGPUDevice(
            SDL_GPU_SHADERFORMAT_DXIL,
            enableDebug,
            "direct3d12"
        );

        if (!m_device)
        {
            const std::string error = SDL_GetError();
            LogDiagnostic(
                stageName +
                "_device_create_failed requested_backend=\"direct3d12\" error=\"" +
                error +
                "\" terminal_failure=true"
            );
            std::cerr
                << "Failed to create " << stageName << " GPU device: "
                << error
                << '\n';
            return false;
        }

        const char* gpuDriver = SDL_GetGPUDeviceDriver(m_device);
        LogDiagnostic(
            stageName +
            "_device_created requested_backend=\"direct3d12\" actual_backend=\"" +
            (gpuDriver ? gpuDriver : "unavailable") +
            "\""
        );
        m_presentationDiagnostics->DrainDxgiMessages(
            (stageName + "_device_created").c_str(),
            [this](const std::string& entry) { LogDiagnostic(entry); }
        );

        if (!SDL_ClaimWindowForGPUDevice(m_device, m_window))
        {
            const std::string error = SDL_GetError();
            LogDiagnostic(
                stageName +
                "_window_claim_failed error=\"" +
                error +
                "\" terminal_failure=true"
            );
            std::cerr
                << "Failed to claim window for " << stageName << " GPU device: "
                << error
                << '\n';

            LogDiagnostic(stageName + "_unclaimed_device_destroy_started");
            SDL_DestroyGPUDevice(m_device);
            m_device = nullptr;
            LogDiagnostic(stageName + "_unclaimed_device_destroyed");
            m_presentationDiagnostics->DrainDxgiMessages(
                (stageName + "_claim_failed").c_str(),
                [this](const std::string& entry) { LogDiagnostic(entry); }
            );
            return false;
        }

        m_windowClaimed = true;
        LogWindowState(stageName + "_window_claim_succeeded");
        m_presentationDiagnostics->CaptureTopology(
            (stageName + "_window_claim_succeeded").c_str(),
            true,
            [this](const std::string& entry) { LogDiagnostic(entry); }
        );
        m_presentationDiagnostics->DrainDxgiMessages(
            (stageName + "_window_claim_succeeded").c_str(),
            [this](const std::string& entry) { LogDiagnostic(entry); }
        );
        return true;
    }

    void Renderer::OpenDiagnosticLog()
    {
        char* preferencePath = SDL_GetPrefPath("AtomEngine", "AtomGame");
        if (!preferencePath)
        {
            std::cerr
                << "Could not determine swapchain diagnostic log path: "
                << SDL_GetError()
                << '\n';
            return;
        }

        const std::string logPath =
            std::string(preferencePath) + "d3d12-swapchain-diagnostic.log";
        SDL_free(preferencePath);

        m_diagnosticLog = SDL_IOFromFile(logPath.c_str(), "ab");
        if (!m_diagnosticLog)
        {
            std::cerr
                << "Could not open swapchain diagnostic log: "
                << logPath
                << ": "
                << SDL_GetError()
                << '\n';
            return;
        }

        std::cout
            << "Swapchain diagnostic log: "
            << logPath
            << '\n';
    }

    void Renderer::LogDiagnostic(const std::string& message)
    {
        SDL_Time utcTime = 0;
        SDL_GetCurrentTime(&utcTime);

        std::ostringstream entry;
        entry
            << "[utc_ns=" << utcTime
            << " ticks_ms=" << SDL_GetTicks()
            << "] " << message;

        std::clog
            << "[SwapchainDiagnostic] "
            << entry.str()
            << '\n';

        if (m_diagnosticLog)
        {
            const std::string line = entry.str() + '\n';
            const size_t written = SDL_WriteIO(
                m_diagnosticLog,
                line.data(),
                line.size()
            );

            if (written != line.size())
            {
                std::cerr
                    << "Could not write complete swapchain diagnostic entry: "
                    << SDL_GetError()
                    << '\n';
            }

            if (!SDL_FlushIO(m_diagnosticLog))
            {
                std::cerr
                    << "Could not flush swapchain diagnostic log: "
                    << SDL_GetError()
                    << '\n';
            }
        }
    }

    void Renderer::LogWindowState(const std::string& event)
    {
        if (!m_window)
        {
            LogDiagnostic(event + " window=null");
            return;
        }

        int pixelWidth = 0;
        int pixelHeight = 0;
        const bool hasPixelSize = SDL_GetWindowSizeInPixels(
            m_window,
            &pixelWidth,
            &pixelHeight
        );
        const SDL_DisplayID display = SDL_GetDisplayForWindow(m_window);
        const char* displayName = display ? SDL_GetDisplayName(display) : nullptr;

        std::ostringstream details;
        details
            << event
            << " pixel_size=";

        if (hasPixelSize)
        {
            details << pixelWidth << 'x' << pixelHeight;
        }
        else
        {
            details << "unavailable";
        }

        details
            << " display_id=" << display
            << " display_name=\"" << (displayName ? displayName : "unavailable") << '"'
            << " display_scale=" << SDL_GetWindowDisplayScale(m_window)
            << " window_claimed=" << (m_windowClaimed ? "true" : "false")
            << " recovery_attempted=" << (m_deviceRecoveryAttempted ? "true" : "false")
            << " validation_pending=" << (m_waitingForRecoveredFrame ? "true" : "false");

        LogDiagnostic(details.str());
    }

    Renderer::~Renderer()
    {
        Shutdown();
    }

    bool Renderer::Initialize(SDL_Window* window)
    {
#ifndef NDEBUG
        constexpr const char* buildConfiguration = "Debug";
#else
        constexpr const char* buildConfiguration = "Release";
#endif

        m_deviceRecoveryAttempted = false;
        m_waitingForRecoveredFrame = false;
        m_windowClaimed = false;
        m_window = window;
        OpenDiagnosticLog();

        std::ostringstream session;
        session
            << "=== session_start"
            << " build=" << buildConfiguration
            << " sdl_revision=\"" << SDL_GetRevision() << "\""
            << " ===";
        LogDiagnostic(session.str());

        m_presentationDiagnostics->Initialize(
            m_window,
            [this](const std::string& entry) { LogDiagnostic(entry); }
        );

        if (!CreateAndClaimGPUDevice("initial"))
        {
            return false;
        }

        std::cout
            << "GPU device created with backend: "
            << SDL_GetGPUDeviceDriver(m_device)
            << "\nWindow claimed for GPU device.\n";

        return true;
    }

    bool Renderer::Render()
    {
        m_presentationDiagnostics->CaptureTopology(
            "render_topology_changed",
            false,
            [this](const std::string& entry) { LogDiagnostic(entry); }
        );

        if (!m_device || !m_window || !m_windowClaimed)
        {
            LogDiagnostic(
                "render_state_invalid device=" +
                std::string(m_device ? "present" : "null") +
                " window=" +
                (m_window ? "present" : "null") +
                " window_claimed=" +
                (m_windowClaimed ? "true" : "false") +
                " terminal_failure=true"
            );
            return false;
        }

        SDL_GPUCommandBuffer* commandBuffer =
            SDL_AcquireGPUCommandBuffer(m_device);

        if (!commandBuffer)
        {
            // Failing to acquire a command buffer is not part of the normal
            // resize/minimize flow (that case is handled below once the
            // swapchain texture is acquired), so treat this as an
            // unrecoverable error (e.g. the GPU device was removed/reset)
            // and let the caller stop, instead of retrying every frame
            // forever with the same error.
            const std::string error = SDL_GetError();
            LogDiagnostic("command_buffer_acquire_failed error=\"" + error + "\"");
            std::cerr
                << "Failed to acquire GPU command buffer: "
                << error
                << '\n';

            return false;
        }

        SDL_GPUTexture* swapchainTexture = nullptr;

        if (!SDL_WaitAndAcquireGPUSwapchainTexture(
            commandBuffer,
            m_window,
            &swapchainTexture,
            nullptr,
            nullptr))
        {
            const std::string acquisitionError = SDL_GetError();
            LogDiagnostic(
                "swapchain_acquire_failed error=\"" +
                acquisitionError +
                "\""
            );
            LogWindowState("swapchain_acquire_failure_state");
            m_presentationDiagnostics->CaptureTopology(
                "swapchain_acquire_failure",
                true,
                [this](const std::string& entry) { LogDiagnostic(entry); }
            );
            m_presentationDiagnostics->DrainDxgiMessages(
                "swapchain_acquire_failure",
                [this](const std::string& entry) { LogDiagnostic(entry); }
            );

            if (!SDL_CancelGPUCommandBuffer(commandBuffer))
            {
                const std::string cancellationError = SDL_GetError();
                LogDiagnostic(
                    "failed_command_buffer_cancel_failed error=\"" +
                    cancellationError +
                    "\" terminal_failure=true"
                );
                return false;
            }
            LogDiagnostic("failed_command_buffer_cancelled");

            if (m_deviceRecoveryAttempted)
            {
                LogDiagnostic("device_recovery_not_repeated terminal_failure=true");
                return false;
            }

            m_deviceRecoveryAttempted = true;
            LogDiagnostic("device_recovery_started attempt=1 max_attempts=1");

            LogDiagnostic("old_window_release_started");
            SDL_ReleaseWindowFromGPUDevice(m_device, m_window);
            m_windowClaimed = false;
            LogDiagnostic("old_window_released");

            LogDiagnostic("old_device_destroy_started");
            SDL_DestroyGPUDevice(m_device);
            m_device = nullptr;
            LogDiagnostic("old_device_destroyed");
            m_presentationDiagnostics->DrainDxgiMessages(
                "old_device_destroyed",
                [this](const std::string& entry) { LogDiagnostic(entry); }
            );

            LogDiagnostic(
                "replacement_device_resources_recreate_started "
                "persistent_textures=0 persistent_buffers=0 shaders=0 pipelines=0"
            );

            if (!CreateAndClaimGPUDevice("replacement"))
            {
                return false;
            }

            LogDiagnostic(
                "replacement_device_resources_recreated count=0 "
                "temporary_assumption=no_persistent_device_owned_resources"
            );

            m_waitingForRecoveredFrame = true;
            LogWindowState("post_recovery_frame_validation_pending");
            return true;
        }

         /* Solved: When resizing or minimized, the swapchain texture may be null even if acquisition 
         succeeds. Submitting the command buffer empty without drawing avoids displaying stale/ghost 
         frames or using an invalid render target. */
        if (!swapchainTexture)
        {
            if (!SDL_SubmitGPUCommandBuffer(commandBuffer))
            {
                const std::string error = SDL_GetError();
                LogDiagnostic(
                    "empty_frame_submit_failed error=\"" +
                    error +
                    "\""
                );
                return !m_waitingForRecoveredFrame;
            }

            if (m_waitingForRecoveredFrame)
            {
                LogDiagnostic("post_recovery_acquire_succeeded texture=null validation_pending=true");
            }
            return true;
        }

        if (m_waitingForRecoveredFrame)
        {
            LogDiagnostic("post_recovery_acquire_succeeded texture=non_null");
        }

        SDL_GPUColorTargetInfo colorTarget{};
        colorTarget.texture = swapchainTexture;
        colorTarget.clear_color = SDL_FColor{
            0.1f,
            0.15f,
            0.2f,
            1.0f
        };
        colorTarget.load_op = SDL_GPU_LOADOP_CLEAR;
        colorTarget.store_op = SDL_GPU_STOREOP_STORE;

        SDL_GPURenderPass* renderPass =
            SDL_BeginGPURenderPass(
                commandBuffer,
                &colorTarget,
                1,
                nullptr
            );

        if (!renderPass)
        {
            const std::string error = SDL_GetError();
            LogDiagnostic(
                "render_pass_begin_failed error=\"" +
                error +
                "\""
            );
            std::cerr
                << "Failed to begin GPU render pass: "
                << error
                << '\n';

            const bool submitted = SDL_SubmitGPUCommandBuffer(commandBuffer);
            if (!submitted)
            {
                const std::string submitError = SDL_GetError();
                LogDiagnostic(
                    "failed_render_pass_cleanup_submit_failed error=\"" +
                    submitError +
                    "\""
                );
            }
            return !m_waitingForRecoveredFrame;
        }

        SDL_EndGPURenderPass(renderPass);

        if (!SDL_SubmitGPUCommandBuffer(commandBuffer))
        {
            const std::string error = SDL_GetError();
            LogDiagnostic("frame_submit_failed error=\"" + error + "\"");
            std::cerr
                << "Failed to submit GPU command buffer: "
                << error
                << '\n';

            return !m_waitingForRecoveredFrame;
        }

        if (m_waitingForRecoveredFrame)
        {
            m_waitingForRecoveredFrame = false;
            LogWindowState("post_recovery_frame_submitted recovery_validated=true");
        }

        return true;
    }
    
    void Renderer::Shutdown()
    {
        if (m_device || m_window)
        {
            LogDiagnostic(
                "renderer_shutdown_started device=" +
                std::string(m_device ? "present" : "null") +
                " window=" +
                (m_window ? "present" : "null") +
                " window_claimed=" +
                (m_windowClaimed ? "true" : "false")
            );
        }

        if (m_device)
        {
            m_presentationDiagnostics->CaptureTopology(
                "renderer_shutdown",
                true,
                [this](const std::string& entry) { LogDiagnostic(entry); }
            );
            m_presentationDiagnostics->DrainDxgiMessages(
                "renderer_shutdown_before_release",
                [this](const std::string& entry) { LogDiagnostic(entry); }
            );

            if (m_windowClaimed && m_window)
            {
                LogDiagnostic("shutdown_window_release_started");
                SDL_ReleaseWindowFromGPUDevice(
                    m_device,
                    m_window
                );
                m_windowClaimed = false;
                LogDiagnostic("shutdown_window_released");
            }

            LogDiagnostic("shutdown_device_destroy_started");
            SDL_DestroyGPUDevice(m_device);
            m_device = nullptr;
            LogDiagnostic("shutdown_device_destroyed");
        }

        m_presentationDiagnostics->Shutdown(
            [this](const std::string& entry) { LogDiagnostic(entry); }
        );

        m_windowClaimed = false;
        m_window = nullptr;
        m_waitingForRecoveredFrame = false;

        if (m_diagnosticLog)
        {
            LogDiagnostic("=== session_end ===");
            if (!SDL_CloseIO(m_diagnosticLog))
            {
                std::cerr
                    << "Could not close swapchain diagnostic log: "
                    << SDL_GetError()
                    << '\n';
            }
            m_diagnosticLog = nullptr;
        }
    }
}
