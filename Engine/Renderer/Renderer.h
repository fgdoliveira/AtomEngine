#pragma once

struct SDL_Window;

namespace Atom
{
    class Renderer
    {
    public:
        Renderer() = default;
        ~Renderer() = default;

        Renderer(const Renderer&) = delete;
        Renderer& operator=(const Renderer&) = delete;

        bool Initialize(SDL_Window* window);
        void Render();
        void Shutdown();
    };
}