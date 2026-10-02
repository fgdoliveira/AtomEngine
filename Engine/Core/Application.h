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

    protected:
        // Game hooks. OnInitialize runs after the engine is ready,
        // OnShutdown before the engine tears down.
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

        bool m_running = false;
        int m_exitCode = 0;
        bool m_initialized = false;

        Window m_window;
        Renderer m_renderer;
        AudioSystem m_audio;
        Input m_input;
        DevTools m_devTools;
        Time m_time;
    };
}
