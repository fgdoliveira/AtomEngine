#pragma once

#include <functional>
#include <memory>
#include <string>

struct SDL_Window;

namespace Atom
{
    class D3D12PresentationDiagnostics
    {
    public:
        using LogFunction = std::function<void(const std::string&)>;

        D3D12PresentationDiagnostics();
        ~D3D12PresentationDiagnostics();

        D3D12PresentationDiagnostics(
            const D3D12PresentationDiagnostics&
        ) = delete;
        D3D12PresentationDiagnostics& operator=(
            const D3D12PresentationDiagnostics&
        ) = delete;

        void Initialize(SDL_Window* window, const LogFunction& log);
        void CaptureTopology(
            const char* stage,
            bool force,
            const LogFunction& log
        );
        void DrainDxgiMessages(const char* stage, const LogFunction& log);
        void Shutdown(const LogFunction& log);

    private:
        struct Implementation;
        std::unique_ptr<Implementation> m_implementation;
    };
}
