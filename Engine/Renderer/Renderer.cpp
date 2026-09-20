#include "Renderer/Renderer.h"
#include <SDL3/SDL.h>
#include <iostream>

namespace Atom
{
    Renderer::~Renderer()
    {
        Shutdown();
    }

    bool Renderer::Initialize(SDL_Window* window)
    {
#ifndef NDEBUG
        constexpr bool enableDebug = true;
#else
        constexpr bool enableDebug = false;
#endif
		// Create the GPU device with the specified shader format and debug mode
        m_device = SDL_CreateGPUDevice(
            SDL_GPU_SHADERFORMAT_DXIL,
            enableDebug,
            nullptr
        );

        if (!m_device)
        {
            std::cerr
                << "Failed to create GPU device: "
                << SDL_GetError()
                << '\n';

            return false;
        }

        std::cout
            << "GPU device created.\n";

        std::cout
            << "GPU backend: "
            << SDL_GetGPUDeviceDriver(m_device)
            << '\n';


		// Claim the window for the GPU device
        if (!SDL_ClaimWindowForGPUDevice(m_device, window))
        {
            std::cerr
                << "Failed to claim window for GPU device: "
                << SDL_GetError()
                << '\n';

            SDL_DestroyGPUDevice(m_device);
            m_device = nullptr;

            return false;
        }

		// Store the window pointer for later use
        m_window = window;

        std::cout << "Window claimed for GPU device.\n";

        return true;
    }

    bool Renderer::Render()
    {
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
            nullptr))
        {
            /* A failed swapchain acquisition is usually transient:
               the swapchain may be mid-recreate after a resize or a
               monitor change (e.g. moving the window between displays
               with different DPI). Cancel this frame's command buffer
               and retry on the next frame instead of shutting down. */
            SDL_CancelGPUCommandBuffer(commandBuffer);
            return true;
        }

         /* Solved: When resizing or minimized, the swapchain texture may be null even if acquisition 
         succeeds. Submitting the command buffer empty without drawing avoids displaying stale/ghost 
         frames or using an invalid render target. */
        if (!swapchainTexture)
        {
            SDL_SubmitGPUCommandBuffer(commandBuffer);
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

        SDL_GPURenderPass* renderPass =
            SDL_BeginGPURenderPass(
                commandBuffer,
                &colorTarget,
                1,
                nullptr
            );

        if (!renderPass)
        {
            std::cerr
                << "Failed to begin GPU render pass: "
                << SDL_GetError()
                << '\n';

            SDL_SubmitGPUCommandBuffer(commandBuffer);
            return true;
        }

        SDL_EndGPURenderPass(renderPass);

        if (!SDL_SubmitGPUCommandBuffer(commandBuffer))
        {
            std::cerr
                << "Failed to submit GPU command buffer: "
                << SDL_GetError()
                << '\n';
        }

        return true;
    }
    
    void Renderer::Shutdown()
    {
        if (m_device)
        {
            if (m_window)
            {
                SDL_ReleaseWindowFromGPUDevice(
                    m_device,
                    m_window
                );

                m_window = nullptr;
            }

            SDL_DestroyGPUDevice(m_device);
            m_device = nullptr;
        }
    }
}