#pragma once

struct SDL_Window;

namespace Atom
{
    class Window
    {
    public:
        Window() = default;
        ~Window();

		// Delete copy constructor and copy assignment operator
        Window(const Window&) = delete;
        Window& operator=(const Window&) = delete;

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