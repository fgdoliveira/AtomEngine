#include "Core/Application.h"
#include "Core/DevSwitch.h" // M82: ATOM_* switches, compiled out of packages
#include "Core/Profile.h" // v0.0.15: CPU profiling zones (off: nothing)
#include "Core/Trace.h"   // M96: the minimal tracer, started and written here when chosen

#include <SDL3/SDL.h>

#include <algorithm>
#include <cstdio>
#include <iostream>
#include <string>

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
        if (const char* position = Atom::DevSwitch("ATOM_WINDOW_POSITION"); position && *position)
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
        const char* vsync = Atom::DevSwitch("ATOM_VSYNC");
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

        // Developer tools (M41): F10 panels and, M82, the F1 overlay. Without
        // them the game still runs; a distribution build never starts them.
        if (Atom::DevToolsEnabled && m_devTools.Initialize(m_window.GetSDLWindow(), m_renderer.GetDevice()))
        {
            m_renderer.SetOverlayPass([this](SDL_GPUCommandBuffer* commandBuffer, SDL_GPUTexture* target) {
                m_devTools.Render(commandBuffer, target);
            });
        }
        std::cout << (Atom::DevToolsEnabled ? "Developer tools: on (F1 overlay, F10 panels, ATOM_* switches)\n"
                                            : "Developer tools: off (distribution build)\n");

        m_initialized = true;
        m_running = true;

        std::cout << "AtomEngine initialized.\n";
#if ATOM_PROFILE_TRACE
        // M96 (-DATOM_TRACE=ON): ATOM_TRACE=<file prefix> records the
        // profiling zones from here to shutdown (Core/Trace.h).
        if (const char* trace = Atom::DevSwitch("ATOM_TRACE"); trace && *trace)
        {
            m_tracePrefix = trace;
            Trace::Start();
            std::cout << "Trace: recording to " << m_tracePrefix << ".json\n";
        }
#endif

        if (!OnInitialize())
        {
            Shutdown();
            return false;
        }
        {
            // M98: the app has applied its settings and loaded what it
            // starts with: build the renderer's pipelines now, not on the
            // first frames. ATOM_PREWARM=0 skips it (to measure the difference).
            const char* prewarm = Atom::DevSwitch("ATOM_PREWARM");
            if (!(prewarm && SDL_strcmp(prewarm, "0") == 0))
            {
                const std::uint64_t start = SDL_GetTicksNS();
                m_renderer.Prewarm();
                // The first frames' own first-use costs were the rest of the
                // hitch: the driver's first submission of the scene's
                // resources, the app's first update, and - one frame later -
                // the second set of per-frame buffers SDL creates when the
                // first is still in flight. So draw as many frames as can be
                // in flight here, with no time passing, and wait for the GPU.
                const std::uint32_t warmFrames = std::max<std::uint32_t>(1, m_renderer.GetDeviceReport().framesInFlight);
                for (std::uint32_t i = 0; i < warmFrames && m_running; ++i)
                {
                    ATOM_PROFILE_SCOPE("PrewarmFrame");
                    OnUpdate(0.0f);
                    if (!m_renderer.Render())
                    {
                        break;
                    }
                    ATOM_PROFILE_FRAME(); // frames of their own in a trace: the loading ones
                }
                SDL_WaitForGPUIdle(m_renderer.GetDevice());
                std::cout << "Prewarm: " << static_cast<double>(SDL_GetTicksNS() - start) / 1e6 << " ms\n";
            }
        }

        m_time.Reset();

        return true;
    }

    int Application::Run()
    {
        if (!Initialize())
        {
            m_startFailed = true; // M67: the caller can tell the player why
            return 1;
        }

        while (m_running)
        {
            // M74 (ATOM_LATENCY_WAIT=early): wait for the swapchain first, so
            // input is read after the wait rather than before it.
            const std::uint64_t waitStartNs = SDL_GetTicksNS(); // M99: the frame's timing
            {
                ATOM_PROFILE_SCOPE("WaitForPresentSlot");
                m_renderer.WaitForPresentSlot();
            }
            // M73 (ATOM_LATENCY_LOG): when the frame began, and the click it read.
            const std::uint64_t frameStartNs = SDL_GetTicksNS();
            {
                ATOM_PROFILE_SCOPE("Events");
                ProcessEvents();
            }
            m_renderer.GetLatencyProbe().BeginFrame(frameStartNs, m_input.GetLeftClickTimeNS());
            m_devTools.BeginFrame();
            const float deltaSeconds = m_time.Tick();
            if (m_devTools.IsOverlayVisible())
            {
                AddEngineOverlayLines(deltaSeconds); // M82: the game appends its own in OnUpdate
            }

            {
                ATOM_PROFILE_SCOPE("Update"); // the app's frame
                OnUpdate(deltaSeconds);
            }

            if (!m_running)
            {
                break;
            }

            const std::uint64_t renderStartNs = SDL_GetTicksNS();
            if (!m_renderer.Render())
            {
                std::cerr
                    << "Renderer encountered a fatal error. Shutting down.\n";

                OnRenderFailure(m_renderer.GetLastFailure()); // M63
                m_exitCode = 3; // not a clean exit: scripts and ctest can tell
                m_running = false;
            }
            // M99: where the frame's time went - the CPU's work, and its
            // waits for the GPU (before the frame, and inside Render for a
            // swapchain image).
            const std::uint64_t endNs = SDL_GetTicksNS();
            const std::uint64_t acquireNs = std::min(m_renderer.GetLastAcquireWaitNs(), endNs - renderStartNs);
            m_frameTiming.waitMs = static_cast<double>(frameStartNs - waitStartNs + acquireNs) / 1e6;
            m_frameTiming.updateMs = static_cast<double>(renderStartNs - frameStartNs) / 1e6;
            m_frameTiming.renderMs = static_cast<double>(endNs - renderStartNs - acquireNs) / 1e6;
            m_frameTimingAverage.Add(m_frameTiming);
            ATOM_PROFILE_FRAME(); // v0.0.15: a frame ends here
        }

        Shutdown();

        return m_exitCode;
    }

    void Application::AddEngineOverlayLines(float deltaSeconds)
    {
        // What every game made with AtomEngine shows under F1 (M82); games
        // append their own lines after these.
        const float frameMs = deltaSeconds * 1000.0f;
        m_smoothedFrameMs += (frameMs - m_smoothedFrameMs) * 0.05f;
        const FrameStats& stats = m_renderer.GetLastFrameStats();
        const Renderer::DeviceReport device = m_renderer.GetDeviceReport();
        std::uint32_t triangles = 0;
        for (const LayerStats& layer : stats.layers)
        {
            triangles += layer.triangles;
        }
        char line[256];
        std::snprintf(line, sizeof(line), "%.2f ms  (%.0f fps)", m_smoothedFrameMs,
                      m_smoothedFrameMs > 0.0f ? 1000.0f / m_smoothedFrameMs : 0.0f);
        m_devTools.AddOverlayLine(line);
        // M99: the CPU's work and its wait for the GPU (averaged).
        const FrameTiming& t = m_frameTimingAverage.value;
        std::snprintf(line, sizeof(line), "CPU %.2f ms (update %.2f, render %.2f)   waiting for GPU %.2f ms", t.CpuMs(),
                      t.updateMs, t.renderMs, t.waitMs);
        m_devTools.AddOverlayLine(line);
        std::snprintf(line, sizeof(line), "Scene %ux%u  MSAA %ux   draws %u / %u   tris %.1fk", stats.sceneWidth,
                      stats.sceneHeight, stats.msaaSamples, stats.drawn, stats.submitted, triangles / 1000.0f);
        m_devTools.AddOverlayLine(line);
        std::snprintf(line, sizeof(line), "Shadow %u   binds: pipelines %u materials %u   particles %u", stats.shadowDrawn,
                      stats.pipelineBinds, stats.materialBinds, stats.particles);
        m_devTools.AddOverlayLine(line);
        m_devTools.AddOverlayLine(device.adapter + "  (" + device.presentMode + ", " + std::to_string(device.framesInFlight)
                                  + " frames in flight)");
        if (const std::string& latency = m_renderer.GetLatencyProbe().LastLine(); !latency.empty())
        {
            m_devTools.AddOverlayLine(latency);
        }
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
                // The panels need a pointer; the game gets the mouse back as
                // it had it when F10 closes them.
                m_capturedBeforeTools = m_input.IsMouseCaptured();
                m_input.SetMouseCaptured(m_window.GetSDLWindow(), false);
            }
            else if (!m_devTools.IsVisible() && wasVisible && m_capturedBeforeTools)
            {
                m_input.SetMouseCaptured(m_window.GetSDLWindow(), true);
                m_capturedBeforeTools = false;
            }

            if (event.type == SDL_EVENT_QUIT)
            {
                m_running = false;
            }
        }

        // M90: a fullscreen switch (or a lost and regained focus) can drop
        // the window's relative mouse mode, and with it the hidden cursor,
        // while the game still counts the mouse as captured: put it back.
        SDL_Window* window = m_window.GetSDLWindow();
        if (m_input.IsMouseCaptured() && !SDL_GetWindowRelativeMouseMode(window)
            && (SDL_GetWindowFlags(window) & SDL_WINDOW_INPUT_FOCUS))
        {
            m_input.SetMouseCaptured(window, true);
        }
    }

    void Application::Shutdown()
    {
        if (!m_initialized)
        {
            return;
        }

        m_initialized = false;
#if ATOM_PROFILE_TRACE
        if (!m_tracePrefix.empty())
        {
            const bool written = Trace::StopAndWrite(m_tracePrefix);
            std::cout << "Trace: " << Trace::Events().size() << " zones over " << Trace::Frames() << " frames"
                      << (Trace::Overflowed() ? " (buffer full)" : "") << (written ? ", written" : ", NOT written") << '\n';
        }
#endif

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
