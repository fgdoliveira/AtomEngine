#include "Renderer/Renderer.h"

#include <SDL3/SDL.h>

#include <iostream>
#include <string>

namespace Atom
{
    namespace
    {
        const char* GetPreferenceName(GPUPreference preference)
        {
            return preference == GPUPreference::LowPower
                ? "low_power"
                : "high_performance";
        }
    }

    bool Renderer::CreateAndClaimGPUDevice(GPUPreference preference)
    {
#ifndef NDEBUG
        constexpr bool enableDebug = true;
#else
        constexpr bool enableDebug = false;
#endif

        const SDL_PropertiesID properties = SDL_CreateProperties();
        if (!properties)
        {
            std::cerr
                << "Failed to create GPU device properties: "
                << SDL_GetError()
                << '\n';
            return false;
        }

        const bool propertiesConfigured =
            SDL_SetStringProperty(
                properties,
                SDL_PROP_GPU_DEVICE_CREATE_NAME_STRING,
                "direct3d12"
            ) &&
            SDL_SetBooleanProperty(
                properties,
                SDL_PROP_GPU_DEVICE_CREATE_SHADERS_DXIL_BOOLEAN,
                true
            ) &&
            SDL_SetBooleanProperty(
                properties,
                SDL_PROP_GPU_DEVICE_CREATE_DEBUGMODE_BOOLEAN,
                enableDebug
            ) &&
            SDL_SetBooleanProperty(
                properties,
                SDL_PROP_GPU_DEVICE_CREATE_PREFERLOWPOWER_BOOLEAN,
                preference == GPUPreference::LowPower
            );

        if (!propertiesConfigured)
        {
            const std::string error = SDL_GetError();
            SDL_DestroyProperties(properties);
            std::cerr
                << "Failed to configure GPU device properties: "
                << error
                << '\n';
            return false;
        }

        m_device = SDL_CreateGPUDeviceWithProperties(properties);
        const std::string creationError = m_device ? "" : SDL_GetError();
        SDL_DestroyProperties(properties);

        if (!m_device)
        {
            std::cerr
                << "Failed to create Direct3D 12 GPU device: "
                << creationError
                << '\n';
            return false;
        }

        if (!SDL_ClaimWindowForGPUDevice(m_device, m_window))
        {
            const std::string error = SDL_GetError();
            std::cerr
                << "Failed to claim window for GPU device: "
                << error
                << '\n';
            SDL_DestroyGPUDevice(m_device);
            m_device = nullptr;
            return false;
        }

        m_windowClaimed = true;

        const char* backend = SDL_GetGPUDeviceDriver(m_device);
        const char* adapter = SDL_GetStringProperty(
            SDL_GetGPUDeviceProperties(m_device),
            SDL_PROP_GPU_DEVICE_NAME_STRING,
            "unavailable"
        );
        std::cout
            << "GPU device: backend="
            << (backend ? backend : "unavailable")
            << " adapter=\"" << adapter << '"'
            << " preference=" << GetPreferenceName(preference)
            << '\n';

        return true;
    }

    Renderer::~Renderer()
    {
        Shutdown();
    }

    bool Renderer::Initialize(
        SDL_Window* window,
        const RendererConfig& config
    )
    {
        Shutdown();

        if (!window)
        {
            std::cerr << "Cannot initialize renderer with a null window.\n";
            return false;
        }

        m_window = window;
        return CreateAndClaimGPUDevice(config.gpuPreference);
    }

    bool Renderer::Render()
    {
        if (!m_device || !m_window || !m_windowClaimed)
        {
            std::cerr << "Cannot render before the renderer is initialized.\n";
            return false;
        }

        SDL_GPUCommandBuffer* commandBuffer =
            SDL_AcquireGPUCommandBuffer(m_device);
        if (!commandBuffer)
        {
            std::cerr
                << "Failed to acquire GPU command buffer: "
                << SDL_GetError()
                << '\n';
            return false;
        }

        SDL_GPUTexture* swapchainTexture = nullptr;
        if (!SDL_WaitAndAcquireGPUSwapchainTexture(
            commandBuffer,
            m_window,
            &swapchainTexture,
            nullptr,
            nullptr
        ))
        {
            const std::string acquisitionError = SDL_GetError();
            const bool cancelled = SDL_CancelGPUCommandBuffer(commandBuffer);
            const std::string cancellationError = cancelled
                ? ""
                : SDL_GetError();

            std::cerr
                << "Failed to acquire GPU swapchain texture: "
                << acquisitionError
                << '\n';
            if (!cancelled)
            {
                std::cerr
                    << "Failed to cancel GPU command buffer: "
                    << cancellationError
                    << '\n';
            }
            return false;
        }

        if (!swapchainTexture)
        {
            if (!SDL_SubmitGPUCommandBuffer(commandBuffer))
            {
                std::cerr
                    << "Failed to submit empty GPU command buffer: "
                    << SDL_GetError()
                    << '\n';
                return false;
            }
            return true;
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

        SDL_GPURenderPass* renderPass = SDL_BeginGPURenderPass(
            commandBuffer,
            &colorTarget,
            1,
            nullptr
        );
        if (!renderPass)
        {
            const std::string renderPassError = SDL_GetError();
            const bool submitted = SDL_SubmitGPUCommandBuffer(commandBuffer);
            const std::string submissionError = submitted
                ? ""
                : SDL_GetError();

            std::cerr
                << "Failed to begin GPU render pass: "
                << renderPassError
                << '\n';
            if (!submitted)
            {
                std::cerr
                    << "Failed to submit GPU command buffer during cleanup: "
                    << submissionError
                    << '\n';
            }
            return false;
        }

        SDL_EndGPURenderPass(renderPass);

        if (!SDL_SubmitGPUCommandBuffer(commandBuffer))
        {
            std::cerr
                << "Failed to submit GPU command buffer: "
                << SDL_GetError()
                << '\n';
            return false;
        }

        return true;
    }

    void Renderer::Shutdown()
    {
        if (m_device)
        {
            if (m_windowClaimed && m_window)
            {
                SDL_ReleaseWindowFromGPUDevice(m_device, m_window);
            }

            SDL_DestroyGPUDevice(m_device);
        }

        m_device = nullptr;
        m_window = nullptr;
        m_windowClaimed = false;
    }
}
