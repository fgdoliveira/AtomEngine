#pragma once

#include "Core/Time.h"
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

        void RequestQuit()
        {
            m_running = false;
        }

        Window& GetWindow() { return m_window; }
        Input& GetInput() { return m_input; }
        Renderer& GetRenderer() { return m_renderer; }
        const Time& GetTime() const { return m_time; }

    private:
        void ProcessEvents();

        bool m_running = false;
        bool m_initialized = false;

        Window m_window;
        Renderer m_renderer;
        Input m_input;
        Time m_time;
    };
}
