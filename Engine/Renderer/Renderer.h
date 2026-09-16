#pragma once

struct SDL_Window;
struct SDL_GPUDevice;

namespace Atom
{
    class Renderer
    {
	public:
		Renderer() = default;
		~Renderer();

		// Delete copy constructor and assignment operator to prevent copying
        Renderer(const Renderer&) = delete;
        Renderer& operator=(const Renderer&) = delete;

        bool Initialize(SDL_Window* window);
        void Render();
        void Shutdown();

    private:

		// Pointer to the SDL GPU device
        SDL_GPUDevice* m_device = nullptr;

		// Pointer to the SDL window
        SDL_Window* m_window = nullptr;
    };
}