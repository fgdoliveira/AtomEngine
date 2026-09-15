#pragma once

struct SDL_Window;

namespace Atom
{
    class Window
    {
    public:
        Window() = default;
        ~Window();

        bool Create(
            const char* title,
            int width,
            int height
        );

        void Destroy();

        SDL_Window* GetSDLWindow() const
        {
            return m_window;
        }

    private:
        SDL_Window* m_window = nullptr;
    };
}