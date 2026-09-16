#pragma once

#include "Platform/Window.h"
#include "Renderer/Renderer.h"

namespace Atom
{
    class Application
    {
    public:
        bool Initialize();
        int Run();
        void Shutdown();

    private:
        void ProcessEvents();

        bool m_running = false;

        Window m_window;
		Renderer m_renderer;
    };
}