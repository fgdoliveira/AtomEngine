#pragma once

#include "Renderer/Mesh.h"

#include <glm/mat4x4.hpp>

#include <cstdint>
#include <memory>
#include <span>
#include <vector>

struct SDL_Window;
struct SDL_GPUDevice;
struct SDL_GPUGraphicsPipeline;
struct SDL_GPUTexture;

namespace Atom
{
    enum class GPUPreference
    {
        LowPower,
        HighPerformance
    };

    struct RendererConfig
    {
        GPUPreference gpuPreference = GPUPreference::LowPower;
    };

    class Renderer
    {
    public:
        Renderer() = default;
        ~Renderer();

        Renderer(const Renderer&) = delete;
        Renderer& operator=(const Renderer&) = delete;

        bool Initialize(
            SDL_Window* window,
            const RendererConfig& config = {}
        );
        bool Render();
        void Shutdown();

        std::unique_ptr<Mesh> CreateMesh(
            std::span<const Vertex> vertices,
            std::span<const std::uint32_t> indices
        );

        // The projection is built at render time from the swapchain size so
        // it always matches the window.
        void SetCamera(
            const glm::mat4& view,
            float verticalFovRadians,
            float nearPlane,
            float farPlane
        );

        // Queues a mesh for this frame. The mesh must outlive Render().
        void Submit(const Mesh& mesh, const glm::mat4& model);

    private:
        struct DrawCommand
        {
            const Mesh* mesh = nullptr;
            glm::mat4 model{1.0f};
        };

        struct Camera
        {
            glm::mat4 view{1.0f};
            float verticalFov = 1.0f;
            float nearPlane = 0.1f;
            float farPlane = 100.0f;
        };

        bool CreateAndClaimGPUDevice(GPUPreference preference);
        bool CreateBasicPipeline();
        bool EnsureDepthTexture(std::uint32_t width, std::uint32_t height);

        SDL_GPUDevice* m_device = nullptr;
        SDL_Window* m_window = nullptr;
        bool m_windowClaimed = false;

        SDL_GPUGraphicsPipeline* m_basicPipeline = nullptr;
        SDL_GPUTexture* m_depthTexture = nullptr;
        std::uint32_t m_depthWidth = 0;
        std::uint32_t m_depthHeight = 0;

        Camera m_camera;
        std::vector<DrawCommand> m_drawCommands;
    };
}
