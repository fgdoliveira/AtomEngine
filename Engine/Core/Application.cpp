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

        m_running = true;

        std::cout << "AtomEngine initialized.\n";

        return true;
    }

    int Application::Run()
    {
        if (!Initialize())
        {
            return 1;
        }

        SDL_Event event;

        while (m_running)
        {
            while (SDL_PollEvent(&event))
            {
                if (event.type == SDL_EVENT_QUIT)
                {
                    m_running = false;
                }
            }
        }

        Shutdown();

        return 0;
    }

    void Application::Shutdown()
    {
        std::cout << "Shutting down AtomEngine...\n";

        m_window.Destroy();

        SDL_Quit();

        m_running = false;
    }
}