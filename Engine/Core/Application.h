#pragma once

#include "Audio/AudioSystem.h"
#include "Core/Time.h"
#include "Debug/DevTools.h"
#include "Platform/Input.h"
#include "Platform/Window.h"
#include "Renderer/Renderer.h"

namespace Atom
{
    class Application
    {
    public:
        virtual ~Application() = default;

        bool Initialize();
        int Run();
        void Shutdown();
        // Run() returned because Initialize failed (M67): the process never
        // reached its first frame, and the player deserves to know why.
        bool StartFailed() const { return m_startFailed; }

    protected:
        // What the game decides before the engine starts (M60): which GPU
        // to prefer must be known when the device is created.
        struct StartupConfig
        {
            GPUPreference gpuPreference = GPUPreference::LowPower;
        };

        // Game hooks. OnConfigure runs first, before the window and GPU
        // exist; OnInitialize runs after the engine is ready, OnShutdown
        // before the engine tears down.
        virtual StartupConfig OnConfigure() { return {}; }
        // A frame failed beyond recovery (M63): called once, before the
        // clean shutdown, so the game can remember a safer choice for the
        // next launch. The engine does not rebuild the device in-process.
        virtual void OnRenderFailure(Renderer::Failure /*failure*/) {}
        virtual bool OnInitialize() { return true; }
        virtual void OnUpdate(float /*deltaSeconds*/) {}
        virtual void OnShutdown() {}

        // exitCode becomes the process exit code (e.g. 1 = a test failed).
        void RequestQuit(int exitCode = 0)
        {
            m_exitCode = exitCode;
            m_running = false;
        }

        Window& GetWindow() { return m_window; }
        Input& GetInput() { return m_input; }
        Renderer& GetRenderer() { return m_renderer; }
        AudioSystem& GetAudio() { return m_audio; }
        DevTools& GetDevTools() { return m_devTools; }
        const Time& GetTime() const { return m_time; }

    private:
        void ProcessEvents();
        // M82: the F1 overlay's engine section (frame, renderer, device).
        void AddEngineOverlayLines(float deltaSeconds);
        float m_smoothedFrameMs = 0.0f;

        bool m_running = false;
        int m_exitCode = 0;
        bool m_startFailed = false;
        bool m_initialized = false;

        Window m_window;
        Renderer m_renderer;
        AudioSystem m_audio;
        Input m_input;
        DevTools m_devTools;
        Time m_time;
    };
}
