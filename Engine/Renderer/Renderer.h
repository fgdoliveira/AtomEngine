#pragma once

#include "Renderer/Glow.h"
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

    // Distance layers of a level (M22): near is walkable detail, mid the
    // simplified buildings around it, far the skyline. Counted separately.
    enum class RenderLayer : std::uint8_t
    {
        Near,
        Mid,
        Far,
    };
    inline constexpr std::size_t RenderLayerCount = 3;

    // A group of draws culled as a whole (a building, half a block): one
    // box test per chunk instead of one per mesh.
    struct ChunkInfo
    {
        glm::vec3 boundsMin{ 0.0f }; // world space
        glm::vec3 boundsMax{ 0.0f };
        RenderLayer layer = RenderLayer::Near;
        bool castsShadow = true;
    };

    struct LayerStats
    {
        std::uint32_t chunks = 0;        // submitted
        std::uint32_t chunksVisible = 0; // in the camera frustum
        std::uint32_t drawn = 0;         // draw calls
        std::uint32_t triangles = 0;
        std::uint32_t shadowDrawn = 0;   // draws in the shadow pass
    };

    struct FrameStats
    {
        std::uint32_t submitted = 0;
        std::uint32_t drawn = 0; // after frustum culling
        std::uint32_t shadowDrawn = 0;
        std::array<LayerStats, RenderLayerCount> layers{};
        std::uint32_t pipelineBinds = 0;
        std::uint32_t materialBinds = 0;
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

        // Draws submitted between BeginChunk and EndChunk belong to that
        // chunk: culled with it, counted in its layer, and left out of the
        // shadow pass when it casts none. Outside a chunk: near, casts
        // shadows, culled per mesh.
        void BeginChunk(const ChunkInfo& chunk);
        void EndChunk() { m_currentChunk = -1; }

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

        // Wind for vertex sway (M19): velocity in m/s, and a time base.
        void SetWind(const glm::vec3& wind, float time) { m_wind = glm::vec4{ wind, time }; }
        float GetFade() const { return m_fade; }

        // 2D overlay drawn on top of the final image (text, panels).
        UIRenderer& GetUI() { return m_ui; }

        // Queues billboards for this frame (sorted and drawn after opaque
        // geometry). The atlas is split into `columns` equal cells.
        void SubmitParticles(std::span<const Particle> particles);
        // Additive glows around lights (M23), same billboards as particles.
        void SubmitHalos(std::span<const Particle> halos);
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
            int chunk = -1; // index into m_chunks, -1 = none
        };

        // Order draws so state changes are rare: decals last (they need the
        // finished surfaces), then by pipeline variant, then by material.
        void SortDrawCommands();

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
        SDL_GPUGraphicsPipeline* GetParticlePipeline(std::uint32_t samples, bool additive = false);
        // Sorts back to front and copies this frame's particles to the GPU.
        bool UploadParticles(SDL_GPUCommandBuffer* commandBuffer);
        void DrawParticles(
            SDL_GPURenderPass* renderPass,
            SDL_GPUCommandBuffer* commandBuffer,
            const glm::mat4& viewProjection
        );
        bool CanUseAlphaToCoverage(std::uint32_t samples) const;
        bool GlowActive() const;

        // Decals: alpha blending, no depth writes, a depth bias toward the
        // camera so they win against the surface they lie on.
        SDL_GPUGraphicsPipeline* GetDecalPipeline(std::uint32_t samples);
        SDL_GPUGraphicsPipeline* CreateScenePipeline(
            std::size_t slot, bool doubleSided, bool alphaToCoverage, bool decal);

        // Scene pipelines depend on the MSAA sample count, face culling
        // and alpha-to-coverage (masked materials with MSAA); built lazily.
        SDL_GPUGraphicsPipeline* GetScenePipeline(
            std::uint32_t samples,
            bool doubleSided = false,
            bool alphaToCoverage = false
        );

        // Light view-projection for the sun, fitted around the camera and
        // snapped to shadow-map texels so shadows don't swim when moving.
        glm::mat4 ComputeLightViewProjection() const;

        // Draws the queued commands visible from `viewProjection`. The scene
        // pass (sceneSamples > 0) binds full materials and switches pipeline
        // per material; the depth-only pass binds only what alpha testing
        // needs. Returns draws issued.
        std::uint32_t DrawQueue(
            SDL_GPURenderPass* renderPass,
            SDL_GPUCommandBuffer* commandBuffer,
            const glm::mat4& viewProjection,
            std::uint32_t sceneSamples
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
        // [samples slot][double-sided][alpha-to-coverage]
        std::array<SDL_GPUGraphicsPipeline*, 12> m_scenePipelines{};
        std::array<SDL_GPUGraphicsPipeline*, 3> m_decalPipelines{}; // per samples slot
        glm::vec4 m_wind{ 0.0f };
        SDL_GPUGraphicsPipeline* m_postPipeline = nullptr;
        SDL_GPUGraphicsPipeline* m_shadowPipeline = nullptr;
        SDL_GPUTexture* m_shadowMap = nullptr;
        SDL_GPUSampler* m_shadowSampler = nullptr; // comparison sampler

        std::array<SDL_GPUGraphicsPipeline*, 3> m_particlePipelines{};
        std::array<SDL_GPUGraphicsPipeline*, 3> m_haloPipelines{};
        std::array<SDL_GPUGraphicsPipeline*, 3> m_skyPipelines{};
        SDL_GPUGraphicsPipeline* GetSkyPipeline(std::uint32_t samples);
        void DrawSky(SDL_GPURenderPass* renderPass, SDL_GPUCommandBuffer* commandBuffer,
                     const glm::mat4& projection);
        Glow m_glow;
        std::vector<Particle> m_halos;
        std::uint32_t m_uploadedHalos = 0;
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
        std::vector<ChunkInfo> m_chunks; // this frame's
        int m_currentChunk = -1;
        FrameStats m_stats;
        std::uint64_t m_frameIndex = 0; // animates film grain
        float m_fade = 0.0f;
    };
}
