#pragma once

#include "Renderer/Material.h"
#include "Renderer/Mesh.h"
#include "Renderer/RenderSettings.h"
#include "Renderer/RenderTargets.h"
#include "Renderer/Texture.h"

#include <glm/mat4x4.hpp>

#include <array>
#include <cstdint>
#include <memory>
#include <span>
#include <vector>

struct SDL_Window;
struct SDL_GPUCommandBuffer;
struct SDL_GPUDevice;
struct SDL_GPUGraphicsPipeline;
struct SDL_GPUSampler;
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
        bool vsync = true;
    };

    struct FrameStats
    {
        std::uint32_t submitted = 0;
        std::uint32_t drawn = 0; // after frustum culling
        std::uint32_t sceneWidth = 0;
        std::uint32_t sceneHeight = 0;
        std::uint32_t msaaSamples = 0;
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

        // pixels: RGBA8, top row first.
        std::unique_ptr<Texture> CreateTexture(
            std::uint32_t width,
            std::uint32_t height,
            const std::uint8_t* pixels,
            bool srgb = true
        );

        // The projection is built at render time from the swapchain size so
        // it always matches the window.
        void SetCamera(
            const glm::mat4& view,
            float verticalFovRadians,
            float nearPlane,
            float farPlane
        );

        // Queues a mesh for this frame. The mesh and material (and its
        // textures) must outlive Render().
        void Submit(
            const Mesh& mesh,
            const Material& material,
            const glm::mat4& model
        );

        const FrameStats& GetLastFrameStats() const { return m_stats; }

        // Takes effect on the next Render(); targets are rebuilt as needed.
        void SetSettings(const RenderSettings& settings);
        const RenderSettings& GetSettings() const { return m_settings; }

    private:
        struct DrawCommand
        {
            const Mesh* mesh = nullptr;
            const Material* material = nullptr;
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
        bool CreateDefaultResources();
        bool CreatePostPipeline();
        // Scene pipelines depend on the MSAA sample count; built lazily.
        SDL_GPUGraphicsPipeline* GetScenePipeline(std::uint32_t samples);

        bool RenderScenePass(SDL_GPUCommandBuffer* commandBuffer);
        bool RenderPostPass(
            SDL_GPUCommandBuffer* commandBuffer,
            SDL_GPUTexture* swapchainTexture
        );

        SDL_GPUDevice* m_device = nullptr;
        SDL_Window* m_window = nullptr;
        bool m_windowClaimed = false;

        // Indexed by log2(samples): 1x, 2x, 4x.
        std::array<SDL_GPUGraphicsPipeline*, 3> m_scenePipelines{};
        SDL_GPUGraphicsPipeline* m_postPipeline = nullptr;
        SDL_GPUSampler* m_sampler = nullptr;      // material textures
        SDL_GPUSampler* m_postSampler = nullptr;  // scene -> swapchain
        std::unique_ptr<Texture> m_whiteTexture;

        RenderSettings m_settings;
        RenderTargets m_targets;

        Camera m_camera;
        std::vector<DrawCommand> m_drawCommands;
        FrameStats m_stats;
    };
}
