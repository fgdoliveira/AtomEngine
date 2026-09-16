#include "Renderer/Renderer.h"
#include <SDL3/SDL.h>
#include <iostream>

namespace Atom
{
    Renderer::~Renderer()
    {
        Shutdown();
    }

    bool Renderer::Initialize(SDL_Window*)
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

		// Log the GPU backend being used
        std::cout
            << "GPU backend: "
            << SDL_GetGPUDeviceDriver(m_device)
            << '\n';

        return true;
    }

    void Renderer::Render()
    {
    }

    void Renderer::Shutdown()
    {
        if (m_device)
        {
			// Destroy the GPU device and set the pointer to nullptr
            SDL_DestroyGPUDevice(m_device);
            m_device = nullptr;
        }
    }
}