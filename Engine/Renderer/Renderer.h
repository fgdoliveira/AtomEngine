#pragma once

#include "Renderer/Lighting.h"
#include "Renderer/Material.h"
#include "Renderer/Mesh.h"
#include "Renderer/Particles.h"
#include "Renderer/RenderSettings.h"
#include "Renderer/RenderTargets.h"
#include "Renderer/Texture.h"
#include "UI/UIRenderer.h"

#include <glm/mat4x4.hpp>

#include <array>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <vector>

struct SDL_Window;
struct SDL_GPUCommandBuffer;
struct SDL_GPURenderPass;
struct SDL_GPUDevice;
struct SDL_GPUGraphicsPipeline;
struct SDL_GPUSampler;
struct SDL_GPUBuffer;
struct SDL_GPUTransferBuffer;
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
        std::uint32_t shadowDrawn = 0;
        std::uint32_t particles = 0;
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
            std::span<const std::uint32_t> indices,
            bool hasBakedLight = false
        );

        // pixels: RGBA8, top row first.
        std::unique_ptr<Texture> CreateTexture(
            std::uint32_t width,
            std::uint32_t height,
            const std::uint8_t* pixels,
            bool srgb = true
        );

        // Loads a PNG/JPG/... file; nullptr (with a message) on failure.
        std::unique_ptr<Texture> LoadTexture(const std::string& path, bool srgb = true);

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

        // 0 = normal, 1 = black. Applied in the post pass, before the UI.
        void SetFade(float fade) { m_fade = fade; }
        float GetFade() const { return m_fade; }

        // 2D overlay drawn on top of the final image (text, panels).
        UIRenderer& GetUI() { return m_ui; }

        // Queues billboards for this frame (sorted and drawn after opaque
        // geometry). The atlas is split into `columns` equal cells.
        void SubmitParticles(std::span<const Particle> particles);
        void SetParticleAtlas(const Texture* atlas, std::uint32_t columns);

        // Takes effect on the next Render(); targets are rebuilt as needed.
        void SetSettings(const RenderSettings& settings);
        const RenderSettings& GetSettings() const { return m_settings; }

        // Lighting and fog for the next Render(). The sky clears to the fog
        // colour so distant geometry dissolves into it.
        void SetLighting(const SceneLighting& lighting) { m_lighting = lighting; }
        const SceneLighting& GetLighting() const { return m_lighting; }

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
        bool CreateShadowResources();
        bool CreateParticleResources();
        SDL_GPUGraphicsPipeline* GetParticlePipeline(std::uint32_t samples);
        // Sorts back to front and copies this frame's particles to the GPU.
        bool UploadParticles(SDL_GPUCommandBuffer* commandBuffer);
        void DrawParticles(
            SDL_GPURenderPass* renderPass,
            SDL_GPUCommandBuffer* commandBuffer,
            const glm::mat4& viewProjection
        );
        // Scene pipelines depend on the MSAA sample count; built lazily.
        SDL_GPUGraphicsPipeline* GetScenePipeline(std::uint32_t samples);

        // Light view-projection for the sun, fitted around the camera and
        // snapped to shadow-map texels so shadows don't swim when moving.
        glm::mat4 ComputeLightViewProjection() const;

        // Draws the queued commands visible from `viewProjection`. Material
        // binding is skipped for depth-only passes. Returns draws issued.
        std::uint32_t DrawQueue(
            SDL_GPURenderPass* renderPass,
            SDL_GPUCommandBuffer* commandBuffer,
            const glm::mat4& viewProjection,
            bool bindMaterials
        );

        bool RenderShadowPass(
            SDL_GPUCommandBuffer* commandBuffer,
            const glm::mat4& lightViewProjection
        );
        bool RenderScenePass(
            SDL_GPUCommandBuffer* commandBuffer,
            const glm::mat4& lightViewProjection
        );
        bool RenderPostPass(
            SDL_GPUCommandBuffer* commandBuffer,
            SDL_GPUTexture* swapchainTexture,
            std::uint32_t outputWidth,
            std::uint32_t outputHeight
        );

        SDL_GPUDevice* m_device = nullptr;
        SDL_Window* m_window = nullptr;
        bool m_windowClaimed = false;

        // Indexed by log2(samples): 1x, 2x, 4x.
        std::array<SDL_GPUGraphicsPipeline*, 3> m_scenePipelines{};
        SDL_GPUGraphicsPipeline* m_postPipeline = nullptr;
        SDL_GPUGraphicsPipeline* m_shadowPipeline = nullptr;
        SDL_GPUTexture* m_shadowMap = nullptr;
        SDL_GPUSampler* m_shadowSampler = nullptr; // comparison sampler

        std::array<SDL_GPUGraphicsPipeline*, 3> m_particlePipelines{};
        SDL_GPUBuffer* m_particleBuffer = nullptr;
        SDL_GPUTransferBuffer* m_particleTransfer = nullptr;
        std::vector<Particle> m_particles;
        std::uint32_t m_uploadedParticles = 0;
        const Texture* m_particleAtlas = nullptr;
        std::uint32_t m_particleAtlasColumns = 1;
        SDL_GPUSampler* m_sampler = nullptr;      // material textures
        SDL_GPUSampler* m_postSampler = nullptr;  // scene -> swapchain
        SDL_GPUSampler* m_lightmapSampler = nullptr; // clamped, few mips
        std::unique_ptr<Texture> m_whiteTexture;

        RenderSettings m_settings;
        UIRenderer m_ui;
        SceneLighting m_lighting;
        RenderTargets m_targets;

        Camera m_camera;
        std::vector<DrawCommand> m_drawCommands;
        FrameStats m_stats;
        std::uint64_t m_frameIndex = 0; // animates film grain
        float m_fade = 0.0f;
    };
}
