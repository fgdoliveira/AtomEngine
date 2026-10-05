#include "Core/Application.h"

#include <SDL3/SDL.h>

#include <iostream>

namespace Atom
{
    bool Application::Initialize()
    {
        std::cout << "Starting AtomEngine " ATOM_VERSION "...\n";

        // SDL's versions first, so a GPU failure below reads as a report:
        // version, device, presentation, fallback.
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

        const StartupConfig startup = OnConfigure();

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
        // ATOM_WINDOW_POSITION=x,y (M63, development): open on a chosen
        // display - e.g. the one the high-performance adapter can't present
        // to - before the GPU claims the window.
        if (const char* position = SDL_getenv("ATOM_WINDOW_POSITION"); position && *position)
        {
            int x = 0;
            int y = 0;
            if (SDL_sscanf(position, "%d,%d", &x, &y) == 2)
            {
                SDL_SetWindowPosition(m_window.GetSDLWindow(), x, y);
            }
        }

        RendererConfig rendererConfig{};
        rendererConfig.gpuPreference = startup.gpuPreference; // the game's choice (M60)

        // ATOM_VSYNC=0 uncaps the frame rate for profiling.
        const char* vsync = SDL_getenv("ATOM_VSYNC");
        rendererConfig.vsync = !(vsync && SDL_strcmp(vsync, "0") == 0);

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

        // Audio is optional: without a device the game runs silently.
        m_audio.Initialize();

        // Developer tools (M41): F10. Without them the game still runs.
        if (m_devTools.Initialize(m_window.GetSDLWindow(), m_renderer.GetDevice()))
        {
            m_renderer.SetOverlayPass([this](SDL_GPUCommandBuffer* commandBuffer, SDL_GPUTexture* target) {
                m_devTools.Render(commandBuffer, target);
            });
        }

        m_initialized = true;
        m_running = true;

        std::cout << "AtomEngine initialized.\n";

        if (!OnInitialize())
        {
            Shutdown();
            return false;
        }

        m_time.Reset();

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
            m_devTools.BeginFrame();

            OnUpdate(m_time.Tick());

            if (!m_running)
            {
                break;
            }

            if (!m_renderer.Render())
            {
                std::cerr
                    << "Renderer encountered a fatal error. Shutting down.\n";

                OnRenderFailure(m_renderer.GetLastFailure()); // M63
                m_exitCode = 3; // not a clean exit: scripts and ctest can tell
                m_running = false;
            }
        }

        Shutdown();

        return m_exitCode;
    }

    void Application::ProcessEvents()
    {
        m_input.BeginFrame();

        SDL_Event event;

        while (SDL_PollEvent(&event))
        {
            const bool wasVisible = m_devTools.IsVisible();
            // The tools see events first; what they use, the game doesn't.
            if (!m_devTools.HandleEvent(event))
            {
                m_input.HandleEvent(event);
            }
            if (m_devTools.IsVisible() && !wasVisible)
            {
                m_input.SetMouseCaptured(m_window.GetSDLWindow(), false); // the panels need a pointer
            }

            if (event.type == SDL_EVENT_QUIT)
            {
                m_running = false;
            }
        }
    }

    void Application::Shutdown()
    {
        if (!m_initialized)
        {
            return;
        }

        m_initialized = false;

        OnShutdown();

        m_renderer.SetOverlayPass({});
        m_devTools.Shutdown();

        std::cout << "Shutting down AtomEngine...\n";

        m_audio.Shutdown();
        m_renderer.Shutdown();
        m_window.Destroy();

        SDL_Quit();

        m_running = false;
    }
}
