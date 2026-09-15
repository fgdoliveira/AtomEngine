#pragma once

#include "Platform/Window.h"

namespace Atom
{
    class Application
    {
    public:
        bool Initialize();
        int Run();
        void Shutdown();

    private:
        bool m_running = false;

        Window m_window;
    };
}