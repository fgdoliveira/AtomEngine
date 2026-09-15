#include "Platform/Window.h"

#include <SDL3/SDL.h>

#include <iostream>

namespace Atom
{
    Window::~Window()
    {
        Destroy();
    }

    bool Window::Create(
        const char* title,
        int width,
        int height
    )
    {
        m_window = SDL_CreateWindow(
            title,
            width,
            height,
            0
        );

        if (!m_window)
        {
            std::cerr
                << "Failed to create SDL window: "
                << SDL_GetError()
                << '\n';

            return false;
        }

        return true;
    }

    void Window::Destroy()
    {
        if (m_window)
        {
            SDL_DestroyWindow(m_window);
            m_window = nullptr;
        }
    }
}