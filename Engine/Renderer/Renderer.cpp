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

    void Renderer::Render()
    {
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