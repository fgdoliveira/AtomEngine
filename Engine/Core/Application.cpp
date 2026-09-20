#include "Core/Application.h"

#include <SDL3/SDL.h>

#include <iostream>

namespace Atom
{
    bool Application::Initialize()
    {
        std::cout << "Starting AtomEngine...\n";

        if (!SDL_Init(SDL_INIT_VIDEO))
        {
            std::cerr
                << "SDL initialization failed: "
                << SDL_GetError()
                << '\n';

            return false;
        }

        if (!m_window.Create(
            "AtomEngine",
            1280,
            720))
        {
            SDL_Quit();
            return false;
        }

        RendererConfig rendererConfig{};
        rendererConfig.gpuPreference = GPUPreference::LowPower;

        if (!m_renderer.Initialize(
            m_window.GetSDLWindow(),
            rendererConfig
        ))
        {
            m_renderer.Shutdown();
            m_window.Destroy();
            SDL_Quit();
            return false;
        }

        m_running = true;

        std::cout << "AtomEngine initialized.\n";

		// Print SDL version information
        const int compiledVersion = SDL_VERSION;
        const int runtimeVersion = SDL_GetVersion();

        std::cout
            << "SDL compiled version: "
            << SDL_VERSIONNUM_MAJOR(compiledVersion) << '.'
            << SDL_VERSIONNUM_MINOR(compiledVersion) << '.'
            << SDL_VERSIONNUM_MICRO(compiledVersion)
            << '\n';

        std::cout
            << "SDL runtime version: "
            << SDL_VERSIONNUM_MAJOR(runtimeVersion) << '.'
            << SDL_VERSIONNUM_MINOR(runtimeVersion) << '.'
            << SDL_VERSIONNUM_MICRO(runtimeVersion)
            << '\n';

        std::cout
            << "SDL revision: "
            << SDL_GetRevision()
            << '\n';

        return true;
    }

    int Application::Run()
    {
        if (!Initialize())
        {
            return 1;
        }

        while (m_running)
        {
            ProcessEvents();

            if (!m_renderer.Render())
            {
                std::cerr
                    << "Renderer encountered a fatal error. Shutting down.\n";

                m_running = false;
            }
        }

        Shutdown();

        return 0;
    }

    void Application::ProcessEvents()
    {
        SDL_Event event;

        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_EVENT_QUIT)
            {
                m_running = false;
            }
        }
    }

    void Application::Shutdown()
    {
        std::cout << "Shutting down AtomEngine...\n";

        m_renderer.Shutdown();
        m_window.Destroy();

        SDL_Quit();

        m_running = false;
    }
}
