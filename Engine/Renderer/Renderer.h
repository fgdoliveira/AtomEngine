#pragma once

#include "Renderer/Glow.h"
#include "Renderer/Lighting.h"
#include "Renderer/Material.h"
#include "Renderer/Mesh.h"
#include "Renderer/Particles.h"
#include "Renderer/RenderSettings.h"
#include "Renderer/RenderTexture.h"
#include "Renderer/RenderTargets.h"
#include "Renderer/Texture.h"
#include "UI/UIRenderer.h"

#include <glm/mat4x4.hpp>

#include <array>
#include <cstdint>
#include <functional>
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
        std::uint32_t spotShadowDrawn = 0; // M43: the spot's shadow pass
        std::uint32_t spotLitDraws = 0;    // M46: scene draws the spot reaches
        std::uint32_t liveLitDraws = 0;    // M46: scene draws a live light reaches
        std::uint32_t liveLights = 0;      // M46: live lights this frame
        std::array<LayerStats, RenderLayerCount> layers{};
        std::uint32_t pipelineBinds = 0;
        std::uint32_t materialBinds = 0;
        std::uint32_t particles = 0;
        std::uint32_t sceneWidth = 0;
        std::uint32_t sceneHeight = 0;
        std::uint32_t msaaSamples = 0;
        std::uint32_t renderTextures = 0;  // M27: drawn into this frame
        std::uint32_t renderTextureDraws = 0; // scene draws sampling one
        std::uint32_t waterDraws = 0;      // M48: water surfaces drawn
        std::uint32_t reflectionDrawn = 0; // M51: draws in the reflection pass
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
            bool hasBakedLight = false,
            std::span<const SkinVertex> skin = {}
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

        // Render-to-texture (M27): drawn each frame before the scene, from
        // whatever its canvas received that frame.
        std::unique_ptr<RenderTexture> CreateRenderTexture(std::uint32_t width, std::uint32_t height);
        void UnregisterRenderTexture(RenderTexture* target);

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

        // Skinned meshes (M35): the joint palette for this frame, stored
        // once and shared by every part of a model; returns its handle for
        // SubmitSkinned. At most MaxPaletteJoints matrices.
        static constexpr std::size_t MaxPaletteJoints = 64;
        std::uint32_t AddPalette(std::span<const glm::mat4> palette);
        void SubmitSkinned(
            const Mesh& mesh,
            const Material& material,
            const glm::mat4& model,
            std::uint32_t palette
        );

        const FrameStats& GetLastFrameStats() const { return m_stats; }

        // Developer overlay (M41): called each frame after the game's UI,
        // with the window's image to draw on. Screenshots and captures are
        // drawn separately, so nothing drawn here ends up in them.
        using OverlayPass = std::function<void(SDL_GPUCommandBuffer*, SDL_GPUTexture*)>;
        void SetOverlayPass(OverlayPass pass) { m_overlayPass = std::move(pass); }
        SDL_GPUDevice* GetDevice() const { return m_device; }

        // Debug (M36): skinned meshes coloured by their joint weights.
        void SetSkinWeightsView(bool on) { m_skinWeightsView = on; }
        bool GetSkinWeightsView() const { return m_skinWeightsView; }

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
        // A live point light for this frame (M25); see LiveLight.
        void SubmitLiveLight(const LiveLight& light);
        // The spot light for this frame (M42), one at most: the flashlight.
        void SubmitSpotLight(const SpotLight& light)
        {
            m_spot = light;
            m_spotActive = true;
        }
        void SetParticleAtlas(const Texture* atlas, std::uint32_t columns);
        // M50: which way streak particles (rain) stretch, for this frame.
        void SetParticleStreak(const glm::vec3& direction) { m_particleStreak = direction; }
        // M51: skip water surfaces (to measure what they cost).
        void SetWaterEnabled(bool enabled) { m_waterEnabled = enabled; }
        // M51: allow the planar reflection where the water asks for it
        // (on by default; off to measure it).
        void SetReflectionEnabled(bool enabled) { m_reflectionEnabled = enabled; }

        // Takes effect on the next Render(); targets are rebuilt as needed.
        void SetSettings(const RenderSettings& settings);
        const RenderSettings& GetSettings() const { return m_settings; }

        // Screenshots (docs): the next frame, as presented (post pass, and
        // the UI overlay if `includeUi`), is written to `path` as a PNG.
        // Nothing extra happens on frames without a request.
        void RequestCapture(const std::string& path, bool includeUi = false);
        bool IsCapturePending() const { return !m_capturePath.empty(); }

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
            int palette = -1; // index into m_paletteRanges when skinned
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
        // The flashlight's fake beam (M45): three planes crossing along the
        // spot's axis, added onto the scene where the spot reaches.
        bool CreateBeamResources();
        SDL_GPUGraphicsPipeline* GetBeamPipeline(std::uint32_t samples);
        void DrawBeam(SDL_GPURenderPass* renderPass, SDL_GPUCommandBuffer* commandBuffer,
                      const glm::mat4& viewProjection);
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
        SDL_GPUGraphicsPipeline* GetDecalPipeline(std::uint32_t samples, bool skinned = false);
        // Water (M48): blended like a decal but without its depth bias,
        // shaded by Water.frag.
        SDL_GPUGraphicsPipeline* GetWaterPipeline(std::uint32_t samples);
        SDL_GPUGraphicsPipeline* CreateScenePipeline(
            std::size_t slot, bool doubleSided, bool alphaToCoverage, bool decal, bool skinned,
            bool water = false, bool rain = false);
        SDL_GPUGraphicsPipeline* CreateShadowPipeline(bool skinned);
        SDL_GPUGraphicsPipeline* GetShadowPipeline(bool skinned)
        {
            SDL_GPUGraphicsPipeline*& pipeline = m_shadowPipelines[skinned ? 1 : 0];
            if (!pipeline)
            {
                pipeline = CreateShadowPipeline(skinned);
            }
            return pipeline;
        }

        // Scene pipelines depend on the MSAA sample count, face culling
        // and alpha-to-coverage (masked materials with MSAA); built lazily.
        SDL_GPUGraphicsPipeline* GetScenePipeline(
            std::uint32_t samples,
            bool doubleSided = false,
            bool alphaToCoverage = false,
            bool skinned = false
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
            std::uint32_t sceneSamples,
            bool spotPass = false,
            bool reflection = false // M51: opaque near-layer draws only, not counted
        );

        bool RenderShadowPass(
            SDL_GPUCommandBuffer* commandBuffer,
            const glm::mat4& lightViewProjection
        );
        // M43: the spot's depth from the lamp, into its own shadow map.
        bool RenderSpotShadowPass(
            SDL_GPUCommandBuffer* commandBuffer,
            const glm::mat4& spotViewProjection
        );
        bool RenderScenePass(
            SDL_GPUCommandBuffer* commandBuffer,
            const glm::mat4& lightViewProjection,
            const glm::mat4& spotViewProjection
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
        // [skinned][samples slot][double-sided][alpha-to-coverage]
        std::array<SDL_GPUGraphicsPipeline*, 48> m_scenePipelines{}; // [rain][skinned][samples][sides][a2c]
        std::array<SDL_GPUGraphicsPipeline*, 12> m_decalPipelines{}; // [rain][skinned][samples slot]
        std::array<SDL_GPUGraphicsPipeline*, 3> m_waterPipelines{}; // [samples slot]
        glm::vec4 m_wind{ 0.0f };
        SDL_GPUGraphicsPipeline* m_postPipeline = nullptr;
        std::array<SDL_GPUGraphicsPipeline*, 2> m_shadowPipelines{}; // [skinned]
        SDL_GPUTexture* m_shadowMap = nullptr;
        SDL_GPUTexture* m_spotShadowMap = nullptr; // M43
        SDL_GPUSampler* m_shadowSampler = nullptr; // comparison sampler

        std::array<SDL_GPUGraphicsPipeline*, 3> m_particlePipelines{};
        std::array<SDL_GPUGraphicsPipeline*, 3> m_haloPipelines{};
        std::array<SDL_GPUGraphicsPipeline*, 3> m_beamPipelines{};
        std::unique_ptr<Mesh> m_beamMesh;
        std::array<SDL_GPUGraphicsPipeline*, 3> m_skyPipelines{};
        SDL_GPUGraphicsPipeline* GetSkyPipeline(std::uint32_t samples);
        bool RenderTextures(SDL_GPUCommandBuffer* commandBuffer);
        bool RenderCapture(SDL_GPUCommandBuffer* commandBuffer, std::uint32_t width, std::uint32_t height);
        void FinishCapture(SDL_GPUFence* fence);
        void DrawSky(SDL_GPURenderPass* renderPass, SDL_GPUCommandBuffer* commandBuffer,
                     const glm::mat4& projection, const glm::mat4& view, std::uint32_t samples);

        // Planar reflection (M51): the near scene mirrored in
        // the water's plane, at half resolution, for the water to sample.
        bool RenderReflectionPass(SDL_GPUCommandBuffer* commandBuffer,
                                  const glm::mat4& lightViewProjection, const glm::mat4& spotViewProjection);
        bool EnsureReflectionTargets(std::uint32_t width, std::uint32_t height);
        bool m_reflectionEnabled = true;
        bool m_reflectionDrawn = false; // this frame
        SDL_GPUTexture* m_reflectionColor = nullptr;
        SDL_GPUTexture* m_reflectionDepth = nullptr;
        std::uint32_t m_reflectionWidth = 0;
        std::uint32_t m_reflectionHeight = 0;
        Glow m_glow;
        std::vector<Particle> m_halos;
        std::array<LiveLight, MaxLiveLights> m_liveLights{};
        std::size_t m_liveLightCount = 0;
        SpotLight m_spot;
        bool m_spotActive = false;
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
        SDL_GPUSampler* m_pixelSampler = nullptr;    // nearest, clamped (render textures)
        std::vector<RenderTexture*> m_renderTextures;

        // Screenshots: an offscreen copy of the presented frame, read back.
        std::string m_capturePath;
        bool m_captureUi = false;
        SDL_GPUTexture* m_captureTexture = nullptr;
        SDL_GPUTransferBuffer* m_captureTransfer = nullptr;
        std::uint32_t m_captureWidth = 0;
        std::uint32_t m_captureHeight = 0;
        bool m_captureRecorded = false; // this frame's command buffer holds a copy
        std::unique_ptr<Texture> m_whiteTexture;

        RenderSettings m_settings;
        UIRenderer m_ui;
        SceneLighting m_lighting;
        glm::vec3 m_particleStreak{ 0.0f, -1.0f, 0.0f };
        bool m_waterEnabled = true;
        RenderTargets m_targets;

        Camera m_camera;
        std::vector<DrawCommand> m_drawCommands;
        struct PaletteRange
        {
            std::uint32_t first = 0;
            std::uint32_t count = 0;
        };
        std::vector<glm::mat4> m_palettes; // this frame's joint matrices
        std::vector<PaletteRange> m_paletteRanges;
        std::vector<ChunkInfo> m_chunks; // this frame's
        int m_currentChunk = -1;
        FrameStats m_stats;
        std::uint64_t m_frameIndex = 0; // animates film grain
        float m_fade = 0.0f;
        bool m_skinWeightsView = false;
        OverlayPass m_overlayPass;
    };
}
