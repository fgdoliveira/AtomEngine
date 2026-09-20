#pragma once

#include <memory>
#include <string>

struct SDL_Window;
struct SDL_GPUDevice;
struct SDL_IOStream;

namespace Atom
{
    class D3D12PresentationDiagnostics;

    class Renderer
    {
	public:
		Renderer();
		~Renderer();

		// Delete copy constructor and assignment operator to prevent copying
        Renderer(const Renderer&) = delete;
        Renderer& operator=(const Renderer&) = delete;

        bool Initialize(SDL_Window* window);
        // Returns false if a fatal, unrecoverable GPU error occurred
        // (e.g. device removed/reset) and the application should stop.
        bool Render();
        void Shutdown();

    private:

        bool CreateAndClaimGPUDevice(const char* stage);
        void OpenDiagnosticLog();
        void LogDiagnostic(const std::string& message);
        void LogWindowState(const std::string& event);

		// Pointer to the SDL GPU device
        SDL_GPUDevice* m_device = nullptr;

		// Pointer to the SDL window
        SDL_Window* m_window = nullptr;

        SDL_IOStream* m_diagnosticLog = nullptr;
        std::unique_ptr<D3D12PresentationDiagnostics> m_presentationDiagnostics;
        bool m_windowClaimed = false;
        bool m_deviceRecoveryAttempted = false;
        bool m_waitingForRecoveredFrame = false;
    };
}
