#include "Renderer/Renderer.h"

#include "Renderer/Shader.h"

#include <stb_image.h>
#include <stb_image_write.h>

#include <SDL3/SDL.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/matrix.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <tuple>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>

namespace Atom
{
    namespace
    {
        // Mirrors the cbuffer in Shaders/Basic.vert.hlsl.
        struct ObjectUniforms
        {
            glm::mat4 viewProjection;
            glm::mat4 model;
        };

        struct Frustum
        {
            glm::vec4 planes[6];
        };

        // Gribb-Hartmann plane extraction for a zero-to-one depth range.
        // Planes point inward; they are left unnormalised, which is fine for
        // the sign tests below.
        Frustum ExtractFrustum(const glm::mat4& m)
        {
            const auto row = [&](int i) {
                return glm::vec4{ m[0][i], m[1][i], m[2][i], m[3][i] };
            };
            const glm::vec4 r0 = row(0);
            const glm::vec4 r1 = row(1);
            const glm::vec4 r2 = row(2);
            const glm::vec4 r3 = row(3);

            return Frustum{ {
                r3 + r0, r3 - r0,
                r3 + r1, r3 - r1,
                r2, r3 - r2
            } };
        }

        bool IsBoxVisible(const Frustum& frustum, const glm::vec3& boxMin, const glm::vec3& boxMax)
        {
            const glm::vec3 center = (boxMin + boxMax) * 0.5f;
            const glm::vec3 extent = (boxMax - boxMin) * 0.5f;
            for (const glm::vec4& plane : frustum.planes)
            {
                const glm::vec3 normal{ plane };
                if (glm::dot(normal, center) + plane.w < -glm::dot(glm::abs(normal), extent))
                {
                    return false;
                }
            }
            return true;
        }

        // A mesh's world-space box (Arvo): centre and half-extents.
        void WorldBox(const Mesh& mesh, const glm::mat4& model, glm::vec3& center, glm::vec3& extent)
        {
            const glm::vec3 localCenter = (mesh.GetBoundsMin() + mesh.GetBoundsMax()) * 0.5f;
            const glm::vec3 localExtent = (mesh.GetBoundsMax() - mesh.GetBoundsMin()) * 0.5f;
            center = glm::vec3(model * glm::vec4(localCenter, 1.0f));
            extent = glm::vec3{ 0.0f };
            for (int axis = 0; axis < 3; ++axis)
            {
                extent += glm::abs(glm::vec3(model[axis])) * localExtent[axis];
            }
        }

        bool IsVisible(
            const Frustum& frustum,
            const Mesh& mesh,
            const glm::mat4& model
        )
        {
            const glm::vec3 localCenter =
                (mesh.GetBoundsMin() + mesh.GetBoundsMax()) * 0.5f;
            const glm::vec3 localExtent =
                (mesh.GetBoundsMax() - mesh.GetBoundsMin()) * 0.5f;

            // World AABB of the transformed box (Arvo).
            const glm::vec3 center = glm::vec3(model * glm::vec4(localCenter, 1.0f));
            glm::vec3 extent{ 0.0f };
            for (int axis = 0; axis < 3; ++axis)
            {
                extent += glm::abs(glm::vec3(model[axis])) * localExtent[axis];
            }

            for (const glm::vec4& plane : frustum.planes)
            {
                const glm::vec3 normal{ plane };
                const float distance = glm::dot(normal, center) + plane.w;
                const float reach = glm::dot(glm::abs(normal), extent);
                if (distance < -reach)
                {
                    return false;
                }
            }
            return true;
        }

        // Mirrors the cbuffers in Shaders/Basic.frag.hlsl.
        struct MaterialUniforms
        {
            glm::vec4 baseColorFactor;
            glm::vec4 emissiveFactor; // w: baked-light weight for this draw
            glm::vec4 lightmap;       // x: intensity, y: weight (0 = none)
            glm::vec4 alpha;          // x: cutoff (0 = opaque), y: alpha-to-coverage,
                                      // z: has emissive texture, w: fog amount
            glm::vec4 surface;        // M42: x shininess, y specular strength
            glm::vec4 lights;         // M46: x live-light bits, y spot reaches (0/1)
        };

        // Mirrors the cbuffer in Shaders/Shadow.frag.hlsl.
        struct ShadowMaterialUniforms
        {
            glm::vec4 alpha; // x: cutoff (0 = opaque), y: base alpha factor
        };

        // Mirrors the cbuffer in Shaders/Post.frag.hlsl.
        struct PostUniforms
        {
            glm::vec4 tint;   // w: enabled
            glm::vec4 params; // exposure, saturation, grain, vignette
            glm::vec4 output; // width, height, frame index, fade
            glm::vec4 glow;   // x: strength
        };

        constexpr std::uint32_t MaxParticles = 4096;

        // Mirrors the cbuffer in Shaders/Particle.vert.hlsl.
        struct ParticleUniforms
        {
            glm::mat4 viewProjection;
            glm::vec4 cameraRight; // w: atlas columns
            glm::vec4 cameraUp;
        };

        std::size_t SampleSlot(std::uint32_t samples)
        {
            return samples >= 4 ? 2 : samples == 2 ? 1 : 0;
        }

        SDL_GPUSampleCount SampleCountFor(std::size_t slot)
        {
            return slot == 2 ? SDL_GPU_SAMPLECOUNT_4
                : slot == 1 ? SDL_GPU_SAMPLECOUNT_2
                : SDL_GPU_SAMPLECOUNT_1;
        }

        constexpr std::uint32_t ShadowMapSize = 2048;
        // The spot's (M43): it covers a cone a few metres long, not 60 m of
        // street, so a quarter of the texels is plenty.
        constexpr std::uint32_t SpotShadowMapSize = 1024;
        constexpr SDL_GPUTextureFormat ShadowMapFormat =
            SDL_GPU_TEXTUREFORMAT_D32_FLOAT;

        struct SceneUniforms
        {
            glm::mat4 lightViewProjection;
            glm::vec4 sunDirection;
            glm::vec4 sunColor;
            glm::vec4 skyColor;
            glm::vec4 groundColor;
            glm::vec4 fogColor;       // w: density
            glm::vec4 cameraPosition; // w: height falloff
            glm::vec4 fogParams;      // x: base height
            glm::vec4 shadowParams;   // enabled, texel uv, ambient share, normal offset
            glm::vec4 time;           // x: seconds, y: live light count
            glm::vec4 liveLightPosition[MaxLiveLights]; // w: radius
            glm::vec4 liveLightColor[MaxLiveLights];
            glm::vec4 spotPosition;  // M42: xyz, w: range
            glm::vec4 spotDirection; // xyz, w: 1 on, 0 off
            glm::vec4 spotColor;     // rgb times intensity, w: specular scale
            glm::vec4 spotCone;      // x: cos outer, y: cos inner
            glm::mat4 spotViewProjection; // M43: its shadow map
            glm::vec4 spotShadow;    // x: on, y: texel size (uv), z: normal offset per metre
        };

        SceneUniforms MakeSceneUniforms(
            const SceneLighting& lighting,
            const glm::mat4& view,
            const glm::mat4& lightViewProjection,
            float time,
            std::span<const LiveLight> liveLights,
            const SpotLight* spot,
            const glm::mat4& spotViewProjection
        )
        {
            // The camera sits at the translation of the inverse view.
            const glm::vec3 cameraPosition{ glm::inverse(view)[3] };

            SceneUniforms uniforms{};
            uniforms.lightViewProjection = lightViewProjection;
            uniforms.shadowParams = glm::vec4{
                lighting.shadowsEnabled ? 1.0f : 0.0f,
                1.0f / static_cast<float>(ShadowMapSize),
                lighting.shadowAmbientShare,
                lighting.shadowNormalOffset
            };
            uniforms.sunDirection =
                glm::vec4{ glm::normalize(lighting.sunDirection), 0.0f };
            uniforms.sunColor = glm::vec4{ lighting.sunColor, 0.0f };
            uniforms.skyColor = glm::vec4{ lighting.skyColor, 0.0f };
            uniforms.groundColor = glm::vec4{ lighting.groundColor, 0.0f };
            uniforms.fogColor =
                glm::vec4{ lighting.fogColor, lighting.fogDensity };
            uniforms.cameraPosition =
                glm::vec4{ cameraPosition, lighting.fogHeightFalloff };
            uniforms.fogParams =
                glm::vec4{ lighting.fogBaseHeight, 0.0f, 0.0f, 0.0f };
            uniforms.time = glm::vec4{ time, static_cast<float>(liveLights.size()), 0.0f, 0.0f };
            for (std::size_t i = 0; i < liveLights.size(); ++i)
            {
                uniforms.liveLightPosition[i] = glm::vec4{ liveLights[i].position, liveLights[i].radius };
                uniforms.liveLightColor[i] = glm::vec4{ liveLights[i].color, 0.0f };
            }
            if (spot)
            {
                // Off is all zeros: the shader multiplies by it rather than
                // branching (a per-pixel branch cost 10 % once, §57).
                uniforms.spotPosition = glm::vec4{ spot->position, std::max(spot->range, 0.01f) };
                uniforms.spotDirection = glm::vec4{ glm::normalize(spot->direction), 1.0f };
                uniforms.spotColor = glm::vec4{ spot->color * spot->intensity, spot->specular };
                const float outer = std::max(spot->outerAngleDegrees, spot->innerAngleDegrees + 0.01f);
                uniforms.spotCone = glm::vec4{
                    std::cos(glm::radians(outer)), std::cos(glm::radians(spot->innerAngleDegrees)), 0.0f, 0.0f };
                uniforms.spotViewProjection = spotViewProjection;
                uniforms.spotShadow = glm::vec4{
                    spot->castsShadows ? 1.0f : 0.0f,
                    1.0f / static_cast<float>(SpotShadowMapSize),
                    spot->shadowNormalOffset, 0.0f };
            }
            else
            {
                uniforms.spotPosition = glm::vec4{ 0.0f, 0.0f, 0.0f, 1.0f };
                uniforms.spotCone = glm::vec4{ 0.0f, 1.0f, 0.0f, 0.0f };
            }
            return uniforms;
        }

        const char* GetPreferenceName(GPUPreference preference)
        {
            return preference == GPUPreference::LowPower
                ? "low_power"
                : "high_performance";
        }
    }

    bool Renderer::CreateAndClaimGPUDevice(GPUPreference preference)
    {
#ifndef NDEBUG
        constexpr bool enableDebug = true;
#else
        constexpr bool enableDebug = false;
#endif

        const SDL_PropertiesID properties = SDL_CreateProperties();
        if (!properties)
        {
            std::cerr
                << "Failed to create GPU device properties: "
                << SDL_GetError()
                << '\n';
            return false;
        }

        const bool propertiesConfigured =
            SDL_SetStringProperty(
                properties,
                SDL_PROP_GPU_DEVICE_CREATE_NAME_STRING,
                "direct3d12"
            ) &&
            SDL_SetBooleanProperty(
                properties,
                SDL_PROP_GPU_DEVICE_CREATE_SHADERS_DXIL_BOOLEAN,
                true
            ) &&
            SDL_SetBooleanProperty(
                properties,
                SDL_PROP_GPU_DEVICE_CREATE_DEBUGMODE_BOOLEAN,
                enableDebug
            ) &&
            SDL_SetBooleanProperty(
                properties,
                SDL_PROP_GPU_DEVICE_CREATE_PREFERLOWPOWER_BOOLEAN,
                preference == GPUPreference::LowPower
            );

        if (!propertiesConfigured)
        {
            const std::string error = SDL_GetError();
            SDL_DestroyProperties(properties);
            std::cerr
                << "Failed to configure GPU device properties: "
                << error
                << '\n';
            return false;
        }

        m_device = SDL_CreateGPUDeviceWithProperties(properties);
        const std::string creationError = m_device ? "" : SDL_GetError();
        SDL_DestroyProperties(properties);

        if (!m_device)
        {
            std::cerr
                << "Failed to create Direct3D 12 GPU device: "
                << creationError
                << '\n';
            return false;
        }

        if (!SDL_ClaimWindowForGPUDevice(m_device, m_window))
        {
            const std::string error = SDL_GetError();
            std::cerr
                << "Failed to claim window for GPU device: "
                << error
                << '\n';
            SDL_DestroyGPUDevice(m_device);
            m_device = nullptr;
            return false;
        }

        m_windowClaimed = true;

        const char* backend = SDL_GetGPUDeviceDriver(m_device);
        const char* adapter = SDL_GetStringProperty(
            SDL_GetGPUDeviceProperties(m_device),
            SDL_PROP_GPU_DEVICE_NAME_STRING,
            "unavailable"
        );
        std::cout
            << "GPU device: backend="
            << (backend ? backend : "unavailable")
            << " adapter=\"" << adapter << '"'
            << " preference=" << GetPreferenceName(preference)
            << '\n';

        return true;
    }

    Renderer::~Renderer()
    {
        Shutdown();
    }

    bool Renderer::Initialize(
        SDL_Window* window,
        const RendererConfig& config
    )
    {
        Shutdown();

        if (!window)
        {
            std::cerr << "Cannot initialize renderer with a null window.\n";
            return false;
        }

        m_window = window;
        if (!CreateAndClaimGPUDevice(config.gpuPreference))
        {
            return false;
        }

        // Shaders work in linear space (textures are sampled as sRGB), so
        // let the swapchain do the linear -> sRGB encode on write.
        SDL_GPUSwapchainComposition composition =
            SDL_GPU_SWAPCHAINCOMPOSITION_SDR_LINEAR;
        if (!SDL_WindowSupportsGPUSwapchainComposition(
            m_device, m_window, composition))
        {
            std::cerr
                << "sRGB swapchain unsupported; colors will look dark.\n";
            composition = SDL_GPU_SWAPCHAINCOMPOSITION_SDR;
        }

        // Without vsync prefer tearing-free MAILBOX, else IMMEDIATE.
        SDL_GPUPresentMode presentMode = SDL_GPU_PRESENTMODE_VSYNC;
        if (!config.vsync)
        {
            // ATOM_PRESENT=immediate (M46): tearing allowed, never waits.
            // MAILBOX is tear-free, but a composited display (an external
            // monitor, here) can still hold it to its refresh rate, which
            // makes frame times unmeasurable.
            const char* present = SDL_getenv("ATOM_PRESENT");
            const bool immediateFirst = present && std::string_view(present) == "immediate";
            for (const SDL_GPUPresentMode mode : {
                immediateFirst ? SDL_GPU_PRESENTMODE_IMMEDIATE : SDL_GPU_PRESENTMODE_MAILBOX,
                immediateFirst ? SDL_GPU_PRESENTMODE_MAILBOX : SDL_GPU_PRESENTMODE_IMMEDIATE })
            {
                if (SDL_WindowSupportsGPUPresentMode(m_device, m_window, mode))
                {
                    presentMode = mode;
                    break;
                }
            }
        }

        if (!SDL_SetGPUSwapchainParameters(
            m_device, m_window, composition, presentMode))
        {
            std::cerr
                << "Failed to set swapchain parameters: "
                << SDL_GetError()
                << '\n';
            return false;
        }

        return m_targets.Initialize(m_device)
            && CreateDefaultResources()
            && CreatePostPipeline()
            && CreateShadowResources()
            && CreateParticleResources()
            && CreateBeamResources()
            && m_glow.Initialize(m_device, m_targets.GetColorFormat())
            && m_ui.Initialize(
                m_device, SDL_GetGPUSwapchainTextureFormat(m_device, m_window))
            && GetScenePipeline(m_targets.ClampSampleCount(m_settings.msaaSamples));
    }

    void Renderer::SetSettings(const RenderSettings& settings)
    {
        m_settings = settings;
        m_settings.renderScale = std::clamp(m_settings.renderScale, 0.1f, 1.0f);
    }

    bool Renderer::CreateDefaultResources()
    {
        SDL_GPUSamplerCreateInfo samplerInfo{};
        samplerInfo.min_filter = SDL_GPU_FILTER_LINEAR;
        samplerInfo.mag_filter = SDL_GPU_FILTER_LINEAR;
        samplerInfo.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR;
        samplerInfo.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
        samplerInfo.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
        samplerInfo.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
        samplerInfo.max_lod = 1000.0f;

        m_sampler = SDL_CreateGPUSampler(m_device, &samplerInfo);

        // Scene upscale: bilinear, no mips, clamped at the edges.
        SDL_GPUSamplerCreateInfo postInfo{};
        postInfo.min_filter = SDL_GPU_FILTER_LINEAR;
        postInfo.mag_filter = SDL_GPU_FILTER_LINEAR;
        postInfo.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
        postInfo.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        postInfo.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        postInfo.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;

        m_postSampler = SDL_CreateGPUSampler(m_device, &postInfo);

        // Lightmaps: charts are packed with a few texels of margin, so
        // clamp at the edges and stop after two mip levels - smaller mips
        // would average neighbouring charts into each other.
        SDL_GPUSamplerCreateInfo lightmapInfo = postInfo;
        lightmapInfo.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR;
        lightmapInfo.max_lod = 2.0f;
        m_lightmapSampler = SDL_CreateGPUSampler(m_device, &lightmapInfo);

        // Render textures (M27): nearest, so screen pixels stay square.
        SDL_GPUSamplerCreateInfo pixelInfo = postInfo;
        pixelInfo.min_filter = SDL_GPU_FILTER_NEAREST;
        pixelInfo.mag_filter = SDL_GPU_FILTER_NEAREST;
        m_pixelSampler = SDL_CreateGPUSampler(m_device, &pixelInfo);

        if (!m_sampler || !m_postSampler || !m_lightmapSampler || !m_pixelSampler)
        {
            std::cerr
                << "Failed to create samplers: "
                << SDL_GetError()
                << '\n';
            return false;
        }

        constexpr std::uint8_t white[4] = { 255, 255, 255, 255 };
        m_whiteTexture = Texture::Create(m_device, 1, 1, white);
        return m_whiteTexture != nullptr;
    }

    bool Renderer::CanUseAlphaToCoverage(std::uint32_t samples) const
    {
        // It needs several samples per pixel to mean anything, and an alpha
        // channel in the target to carry the coverage: the compact HDR
        // format (R11G11B10) has none, so there masked materials fall back
        // to a plain alpha test.
        return samples > 1
            && m_targets.GetColorFormat() == SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT;
    }

    SDL_GPUGraphicsPipeline* Renderer::GetScenePipeline(
        std::uint32_t samples,
        bool doubleSided,
        bool alphaToCoverage,
        bool skinned
    )
    {
        const std::size_t slot = samples >= 4 ? 2 : samples == 2 ? 1 : 0;
        alphaToCoverage = alphaToCoverage && CanUseAlphaToCoverage(samples);
        const std::size_t index = (skinned ? 12 : 0) + slot * 4
            + (doubleSided ? 2 : 0) + (alphaToCoverage ? 1 : 0);
        if (!m_scenePipelines[index])
        {
            m_scenePipelines[index] = CreateScenePipeline(slot, doubleSided, alphaToCoverage, false, skinned);
        }
        return m_scenePipelines[index];
    }

    SDL_GPUGraphicsPipeline* Renderer::GetDecalPipeline(std::uint32_t samples, bool skinned)
    {
        const std::size_t slot = samples >= 4 ? 2 : samples == 2 ? 1 : 0;
        const std::size_t index = (skinned ? 3 : 0) + slot;
        if (!m_decalPipelines[index])
        {
            m_decalPipelines[index] = CreateScenePipeline(slot, false, false, true, skinned);
        }
        return m_decalPipelines[index];
    }

    SDL_GPUGraphicsPipeline* Renderer::CreateScenePipeline(
        std::size_t slot,
        bool doubleSided,
        bool alphaToCoverage,
        bool decal,
        bool skinned
    )
    {
        // Skinned meshes: the same fragment shading, a vertex shader that
        // blends joints first (uniform slot 2 holds the palette).
        SDL_GPUShader* vertexShader = LoadShader(
            m_device,
            skinned ? "Skinned.vert" : "Basic.vert",
            SDL_GPU_SHADERSTAGE_VERTEX,
            ShaderResources{ .uniformBuffers = skinned ? 3u : 2u }
        );
        SDL_GPUShader* fragmentShader = LoadShader(
            m_device,
            "Basic.frag",
            SDL_GPU_SHADERSTAGE_FRAGMENT,
            ShaderResources{ .samplers = 5, .uniformBuffers = 2 }
        );

        if (!vertexShader || !fragmentShader)
        {
            if (vertexShader)
            {
                SDL_ReleaseGPUShader(m_device, vertexShader);
            }
            if (fragmentShader)
            {
                SDL_ReleaseGPUShader(m_device, fragmentShader);
            }
            return nullptr;
        }

        SDL_GPUVertexBufferDescription vertexBuffer{};
        vertexBuffer.slot = 0;
        vertexBuffer.pitch = sizeof(Vertex);
        vertexBuffer.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;

        // A second stream for skinned meshes: joints and weights.
        SDL_GPUVertexBufferDescription vertexBuffers[2]{ vertexBuffer, {} };
        vertexBuffers[1].slot = 1;
        vertexBuffers[1].pitch = sizeof(SkinVertex);
        vertexBuffers[1].input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;

        SDL_GPUVertexAttribute attributes[7]{};
        attributes[5].location = 5;
        attributes[5].buffer_slot = 1;
        attributes[5].format = SDL_GPU_VERTEXELEMENTFORMAT_UBYTE4;
        attributes[5].offset = offsetof(SkinVertex, joints);
        attributes[6].location = 6;
        attributes[6].buffer_slot = 1;
        attributes[6].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
        attributes[6].offset = offsetof(SkinVertex, weights);
        attributes[0].location = 0;
        attributes[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
        attributes[0].offset = offsetof(Vertex, position);
        attributes[1].location = 1;
        attributes[1].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
        attributes[1].offset = offsetof(Vertex, normal);
        attributes[2].location = 2;
        attributes[2].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
        attributes[2].offset = offsetof(Vertex, uv);
        attributes[3].location = 3;
        attributes[3].format = SDL_GPU_VERTEXELEMENTFORMAT_USHORT4_NORM;
        attributes[3].offset = offsetof(Vertex, color);
        attributes[4].location = 4;
        attributes[4].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
        attributes[4].offset = offsetof(Vertex, lightmapUv);

        SDL_GPUColorTargetDescription colorTarget{};
        colorTarget.format = m_targets.GetColorFormat();
        if (decal)
        {
            // Straight alpha over what's there: colour = src*a + dst*(1-a).
            colorTarget.blend_state.enable_blend = true;
            colorTarget.blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
            colorTarget.blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
            colorTarget.blend_state.color_blend_op = SDL_GPU_BLENDOP_ADD;
            colorTarget.blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ZERO;
            colorTarget.blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
            colorTarget.blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
        }

        SDL_GPUGraphicsPipelineCreateInfo createInfo{};
        createInfo.vertex_shader = vertexShader;
        createInfo.fragment_shader = fragmentShader;
        createInfo.vertex_input_state.vertex_buffer_descriptions = vertexBuffers;
        createInfo.vertex_input_state.num_vertex_buffers = skinned ? 2 : 1;
        createInfo.vertex_input_state.vertex_attributes = attributes;
        createInfo.vertex_input_state.num_vertex_attributes = skinned ? 7 : 5;
        createInfo.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
        createInfo.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
        createInfo.rasterizer_state.cull_mode =
            doubleSided ? SDL_GPU_CULLMODE_NONE : SDL_GPU_CULLMODE_BACK;
        createInfo.rasterizer_state.front_face =
            SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
        createInfo.rasterizer_state.enable_depth_clip = true;
        createInfo.multisample_state.sample_count = slot == 2
            ? SDL_GPU_SAMPLECOUNT_4
            : slot == 1 ? SDL_GPU_SAMPLECOUNT_2 : SDL_GPU_SAMPLECOUNT_1;
        // The shader's alpha then decides how many samples a pixel covers:
        // soft, sorted-free edges on leaves instead of stair-steps.
        createInfo.multisample_state.enable_alpha_to_coverage = alphaToCoverage;
        createInfo.depth_stencil_state.enable_depth_test = true;
        // Decals test depth (hidden behind walls) but don't write it: the
        // surface below keeps its depth, and overlapping decals both show.
        createInfo.depth_stencil_state.enable_depth_write = !decal;
        createInfo.depth_stencil_state.compare_op =
            SDL_GPU_COMPAREOP_LESS_OR_EQUAL;
        if (decal)
        {
            // Pulled toward the camera, more on slanted views, so the 2 mm
            // they float above their surface is never lost to precision.
            createInfo.rasterizer_state.enable_depth_bias = true;
            createInfo.rasterizer_state.depth_bias_constant_factor = -2.0f;
            createInfo.rasterizer_state.depth_bias_slope_factor = -1.0f;
        }
        createInfo.target_info.color_target_descriptions = &colorTarget;
        createInfo.target_info.num_color_targets = 1;
        createInfo.target_info.depth_stencil_format =
            RenderTargets::GetDepthFormat();
        createInfo.target_info.has_depth_stencil_target = true;

        SDL_GPUGraphicsPipeline* pipeline =
            SDL_CreateGPUGraphicsPipeline(m_device, &createInfo);

        // Pipelines keep what they need; the shader objects can go.
        SDL_ReleaseGPUShader(m_device, vertexShader);
        SDL_ReleaseGPUShader(m_device, fragmentShader);

        if (!pipeline)
        {
            std::cerr
                << "Failed to create scene pipeline: "
                << SDL_GetError()
                << '\n';
            return nullptr;
        }

        return pipeline;
    }

    bool Renderer::CreatePostPipeline()
    {
        SDL_GPUShader* vertexShader = LoadShader(
            m_device,
            "Fullscreen.vert",
            SDL_GPU_SHADERSTAGE_VERTEX,
            ShaderResources{}
        );
        SDL_GPUShader* fragmentShader = LoadShader(
            m_device,
            "Post.frag",
            SDL_GPU_SHADERSTAGE_FRAGMENT,
            ShaderResources{ .samplers = 2, .uniformBuffers = 1 }
        );

        if (!vertexShader || !fragmentShader)
        {
            if (vertexShader)
            {
                SDL_ReleaseGPUShader(m_device, vertexShader);
            }
            if (fragmentShader)
            {
                SDL_ReleaseGPUShader(m_device, fragmentShader);
            }
            return false;
        }

        SDL_GPUColorTargetDescription colorTarget{};
        colorTarget.format =
            SDL_GetGPUSwapchainTextureFormat(m_device, m_window);

        SDL_GPUGraphicsPipelineCreateInfo createInfo{};
        createInfo.vertex_shader = vertexShader;
        createInfo.fragment_shader = fragmentShader;
        createInfo.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
        createInfo.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
        createInfo.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
        createInfo.target_info.color_target_descriptions = &colorTarget;
        createInfo.target_info.num_color_targets = 1;

        m_postPipeline = SDL_CreateGPUGraphicsPipeline(m_device, &createInfo);

        SDL_ReleaseGPUShader(m_device, vertexShader);
        SDL_ReleaseGPUShader(m_device, fragmentShader);

        if (!m_postPipeline)
        {
            std::cerr
                << "Failed to create post pipeline: "
                << SDL_GetError()
                << '\n';
            return false;
        }

        return true;
    }

    std::unique_ptr<Mesh> Renderer::CreateMesh(
        std::span<const Vertex> vertices,
        std::span<const std::uint32_t> indices,
        bool hasBakedLight,
        std::span<const SkinVertex> skin
    )
    {
        return Mesh::Create(m_device, vertices, indices, hasBakedLight, skin);
    }

    std::unique_ptr<Texture> Renderer::CreateTexture(
        std::uint32_t width,
        std::uint32_t height,
        const std::uint8_t* pixels,
        bool srgb
    )
    {
        return Texture::Create(m_device, width, height, pixels, srgb);
    }

    std::unique_ptr<Texture> Renderer::LoadTexture(const std::string& path, bool srgb)
    {
        int width = 0;
        int height = 0;
        int channels = 0;
        stbi_uc* pixels = stbi_load(path.c_str(), &width, &height, &channels, 4);
        if (!pixels)
        {
            std::cerr << "Failed to load texture '" << path << "': " << stbi_failure_reason() << '\n';
            return nullptr;
        }
        auto texture = CreateTexture(
            static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height), pixels, srgb);
        stbi_image_free(pixels);
        return texture;
    }

    void Renderer::SetCamera(
        const glm::mat4& view,
        float verticalFovRadians,
        float nearPlane,
        float farPlane
    )
    {
        m_camera.view = view;
        m_camera.verticalFov = verticalFovRadians;
        m_camera.nearPlane = nearPlane;
        m_camera.farPlane = farPlane;
    }

    void Renderer::Submit(
        const Mesh& mesh,
        const Material& material,
        const glm::mat4& model
    )
    {
        m_drawCommands.push_back(DrawCommand{ &mesh, &material, model, m_currentChunk });
    }

    std::uint32_t Renderer::AddPalette(std::span<const glm::mat4> palette)
    {
        const std::size_t count = std::min(palette.size(), MaxPaletteJoints);
        m_paletteRanges.push_back(PaletteRange{
            static_cast<std::uint32_t>(m_palettes.size()),
            static_cast<std::uint32_t>(count) });
        m_palettes.insert(m_palettes.end(), palette.begin(), palette.begin() + count);
        return static_cast<std::uint32_t>(m_paletteRanges.size() - 1);
    }

    void Renderer::SubmitSkinned(
        const Mesh& mesh,
        const Material& material,
        const glm::mat4& model,
        std::uint32_t palette
    )
    {
        if (!mesh.IsSkinned() || palette >= m_paletteRanges.size())
        {
            Submit(mesh, material, model);
            return;
        }
        m_drawCommands.push_back(DrawCommand{
            &mesh, &material, model, m_currentChunk, static_cast<int>(palette) });
    }

    void Renderer::BeginChunk(const ChunkInfo& chunk)
    {
        m_currentChunk = static_cast<int>(m_chunks.size());
        m_chunks.push_back(chunk);
    }

    void Renderer::SortDrawCommands()
    {
        const auto key = [](const DrawCommand& c) {
            const Material& m = *c.material;
            return std::tuple{
                m.alphaMode == AlphaMode::Blend,
                m.alphaMode == AlphaMode::Mask,
                m.doubleSided,
                c.palette >= 0,
                c.material,
                c.mesh };
        };
        // Stable: equal keys keep submission order (decals over decals).
        std::stable_sort(m_drawCommands.begin(), m_drawCommands.end(),
            [&](const DrawCommand& a, const DrawCommand& b) { return key(a) < key(b); });
    }

    bool Renderer::Render()
    {
        if (!m_device || !m_window || !m_windowClaimed)
        {
            std::cerr << "Cannot render before the renderer is initialized.\n";
            return false;
        }

        // Draws are only valid for the frame they were submitted in.
        struct ClearOnExit
        {
            std::vector<DrawCommand>& commands;
            std::vector<ChunkInfo>& chunks;
            int& currentChunk;
            UIRenderer& ui;
            std::vector<glm::mat4>& palettes;
            std::vector<PaletteRange>& paletteRanges;
            ~ClearOnExit()
            {
                commands.clear();
                palettes.clear();
                paletteRanges.clear();
                chunks.clear();
                currentChunk = -1;
                ui.EndFrame();
            }
        } clearDrawCommands{ m_drawCommands, m_chunks, m_currentChunk, m_ui, m_palettes, m_paletteRanges };

        SDL_GPUCommandBuffer* commandBuffer =
            SDL_AcquireGPUCommandBuffer(m_device);
        if (!commandBuffer)
        {
            std::cerr
                << "Failed to acquire GPU command buffer: "
                << SDL_GetError()
                << '\n';
            return false;
        }

        SDL_GPUTexture* swapchainTexture = nullptr;
        Uint32 swapchainWidth = 0;
        Uint32 swapchainHeight = 0;
        if (!SDL_WaitAndAcquireGPUSwapchainTexture(
            commandBuffer,
            m_window,
            &swapchainTexture,
            &swapchainWidth,
            &swapchainHeight
        ))
        {
            const std::string acquisitionError = SDL_GetError();
            const bool cancelled = SDL_CancelGPUCommandBuffer(commandBuffer);
            const std::string cancellationError = cancelled
                ? ""
                : SDL_GetError();

            std::cerr
                << "Failed to acquire GPU swapchain texture: "
                << acquisitionError
                << '\n';
            if (!cancelled)
            {
                std::cerr
                    << "Failed to cancel GPU command buffer: "
                    << cancellationError
                    << '\n';
            }
            return false;
        }

        if (!swapchainTexture)
        {
            // Minimised or occluded: nothing to draw into this frame.
            if (!SDL_SubmitGPUCommandBuffer(commandBuffer))
            {
                std::cerr
                    << "Failed to submit empty GPU command buffer: "
                    << SDL_GetError()
                    << '\n';
                return false;
            }
            return true;
        }

        const auto scaled = [&](Uint32 size) {
            return std::max<std::uint32_t>(1, static_cast<std::uint32_t>(
                std::lround(size * m_settings.renderScale)));
        };

        m_stats = FrameStats{};
        m_stats.submitted = static_cast<std::uint32_t>(m_drawCommands.size());
        for (const ChunkInfo& chunk : m_chunks)
        {
            ++m_stats.layers[static_cast<std::size_t>(chunk.layer)].chunks;
        }
        SortDrawCommands();

        // Particles are only valid for the frame they were submitted in.
        struct ClearParticles
        {
            std::vector<Particle>& particles;
            ~ClearParticles() { particles.clear(); }
        } clearParticles{ m_particles };

        const glm::mat4 lightViewProjection = ComputeLightViewProjection();
        const glm::mat4 spotViewProjection = m_spotActive ? SpotMath::ViewProjection(m_spot) : glm::mat4{ 1.0f };

        const bool ok =
            m_targets.Ensure(
                scaled(swapchainWidth),
                scaled(swapchainHeight),
                m_settings.msaaSamples)
            && UploadParticles(commandBuffer)
            && m_ui.Upload(commandBuffer)
            && RenderTextures(commandBuffer)
            && RenderShadowPass(commandBuffer, lightViewProjection)
            && RenderSpotShadowPass(commandBuffer, spotViewProjection)
            && RenderScenePass(commandBuffer, lightViewProjection, spotViewProjection)
            && (!GlowActive()
                || m_glow.Render(commandBuffer, m_targets.GetSceneTexture(),
                                 m_targets.GetWidth(), m_targets.GetHeight(), m_lighting.glowThreshold))
            && RenderPostPass(
                commandBuffer,
                swapchainTexture,
                swapchainWidth,
                swapchainHeight)
            && m_ui.Render(
                commandBuffer,
                swapchainTexture,
                swapchainWidth,
                swapchainHeight)
            && (m_overlayPass ? (m_overlayPass(commandBuffer, swapchainTexture), true) : true)
            && (m_capturePath.empty() || RenderCapture(commandBuffer, swapchainWidth, swapchainHeight));

        ++m_frameIndex;

        // A screenshot needs the GPU to have finished before it can be read.
        if (m_captureRecorded)
        {
            SDL_GPUFence* fence = SDL_SubmitGPUCommandBufferAndAcquireFence(commandBuffer);
            if (!fence)
            {
                std::cerr << "Failed to submit GPU command buffer: " << SDL_GetError() << '\n';
                return false;
            }
            FinishCapture(fence);
            return ok;
        }
        if (!SDL_SubmitGPUCommandBuffer(commandBuffer))
        {
            std::cerr
                << "Failed to submit GPU command buffer: "
                << SDL_GetError()
                << '\n';
            return false;
        }

        return ok;
    }

    bool Renderer::CreateShadowResources()
    {
        constexpr SDL_GPUTextureUsageFlags usage =
            SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET
            | SDL_GPU_TEXTUREUSAGE_SAMPLER;
        if (!SDL_GPUTextureSupportsFormat(
            m_device, ShadowMapFormat, SDL_GPU_TEXTURETYPE_2D, usage))
        {
            std::cerr << "GPU cannot sample a D32 shadow map.\n";
            return false;
        }

        SDL_GPUTextureCreateInfo textureInfo{};
        textureInfo.type = SDL_GPU_TEXTURETYPE_2D;
        textureInfo.format = ShadowMapFormat;
        textureInfo.usage = usage;
        textureInfo.width = ShadowMapSize;
        textureInfo.height = ShadowMapSize;
        textureInfo.layer_count_or_depth = 1;
        textureInfo.num_levels = 1;
        textureInfo.sample_count = SDL_GPU_SAMPLECOUNT_1;
        m_shadowMap = SDL_CreateGPUTexture(m_device, &textureInfo);
        textureInfo.width = SpotShadowMapSize;
        textureInfo.height = SpotShadowMapSize;
        m_spotShadowMap = SDL_CreateGPUTexture(m_device, &textureInfo);

        // Hardware PCF: each tap compares and bilinearly blends 2x2 texels.
        SDL_GPUSamplerCreateInfo samplerInfo{};
        samplerInfo.min_filter = SDL_GPU_FILTER_LINEAR;
        samplerInfo.mag_filter = SDL_GPU_FILTER_LINEAR;
        samplerInfo.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
        samplerInfo.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        samplerInfo.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        samplerInfo.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        samplerInfo.enable_compare = true;
        samplerInfo.compare_op = SDL_GPU_COMPAREOP_LESS_OR_EQUAL;
        m_shadowSampler = SDL_CreateGPUSampler(m_device, &samplerInfo);

        if (!m_shadowMap || !m_spotShadowMap || !m_shadowSampler)
        {
            std::cerr
                << "Failed to create shadow map resources: "
                << SDL_GetError()
                << '\n';
            return false;
        }

        return GetShadowPipeline(false) != nullptr;
    }

    SDL_GPUGraphicsPipeline* Renderer::CreateShadowPipeline(bool skinned)
    {
        SDL_GPUShader* vertexShader = LoadShader(
            m_device,
            skinned ? "ShadowSkinned.vert" : "Shadow.vert",
            SDL_GPU_SHADERSTAGE_VERTEX,
            ShaderResources{ .uniformBuffers = skinned ? 3u : 2u }
        );
        SDL_GPUShader* fragmentShader = LoadShader(
            m_device,
            "Shadow.frag",
            SDL_GPU_SHADERSTAGE_FRAGMENT,
            ShaderResources{ .samplers = 1, .uniformBuffers = 1 }
        );

        if (!vertexShader || !fragmentShader)
        {
            if (vertexShader)
            {
                SDL_ReleaseGPUShader(m_device, vertexShader);
            }
            if (fragmentShader)
            {
                SDL_ReleaseGPUShader(m_device, fragmentShader);
            }
            return nullptr;
        }

        // Same vertex buffers as the scene; position, and the uv that alpha
        // testing needs (leaves cast leaf-shaped shadows).
        SDL_GPUVertexBufferDescription vertexBuffer{};
        vertexBuffer.slot = 0;
        vertexBuffer.pitch = sizeof(Vertex);
        vertexBuffer.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;

        SDL_GPUVertexBufferDescription vertexBuffers[2]{ vertexBuffer, {} };
        vertexBuffers[1].slot = 1;
        vertexBuffers[1].pitch = sizeof(SkinVertex);
        vertexBuffers[1].input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;

        SDL_GPUVertexAttribute attributes[5]{};
        attributes[3].location = 3;
        attributes[3].buffer_slot = 1;
        attributes[3].format = SDL_GPU_VERTEXELEMENTFORMAT_UBYTE4;
        attributes[3].offset = offsetof(SkinVertex, joints);
        attributes[4].location = 4;
        attributes[4].buffer_slot = 1;
        attributes[4].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
        attributes[4].offset = offsetof(SkinVertex, weights);
        attributes[0].location = 0;
        attributes[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
        attributes[0].offset = offsetof(Vertex, position);
        attributes[1].location = 1;
        attributes[1].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
        attributes[1].offset = offsetof(Vertex, uv);
        attributes[2].location = 2; // alpha carries the sway weight
        attributes[2].format = SDL_GPU_VERTEXELEMENTFORMAT_USHORT4_NORM;
        attributes[2].offset = offsetof(Vertex, color);

        SDL_GPUGraphicsPipelineCreateInfo createInfo{};
        createInfo.vertex_shader = vertexShader;
        createInfo.fragment_shader = fragmentShader;
        createInfo.vertex_input_state.vertex_buffer_descriptions = vertexBuffers;
        createInfo.vertex_input_state.num_vertex_buffers = skinned ? 2 : 1;
        createInfo.vertex_input_state.vertex_attributes = attributes;
        createInfo.vertex_input_state.num_vertex_attributes = skinned ? 5 : 3;
        createInfo.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
        createInfo.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
        // Kit pieces include single-sided quads (doors, ground); let both
        // faces cast.
        createInfo.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
        createInfo.rasterizer_state.front_face =
            SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
        // Slope bias pushes steep surfaces away from the light; the shader
        // adds a normal offset for the rest.
        createInfo.rasterizer_state.enable_depth_bias = true;
        createInfo.rasterizer_state.depth_bias_slope_factor = 1.5f;
        createInfo.rasterizer_state.enable_depth_clip = true;
        createInfo.depth_stencil_state.enable_depth_test = true;
        createInfo.depth_stencil_state.enable_depth_write = true;
        createInfo.depth_stencil_state.compare_op =
            SDL_GPU_COMPAREOP_LESS_OR_EQUAL;
        createInfo.target_info.depth_stencil_format = ShadowMapFormat;
        createInfo.target_info.has_depth_stencil_target = true;

        SDL_GPUGraphicsPipeline* pipeline = SDL_CreateGPUGraphicsPipeline(m_device, &createInfo);

        SDL_ReleaseGPUShader(m_device, vertexShader);
        SDL_ReleaseGPUShader(m_device, fragmentShader);

        if (!pipeline)
        {
            std::cerr
                << "Failed to create shadow pipeline: "
                << SDL_GetError()
                << '\n';
        }
        return pipeline;
    }

    glm::mat4 Renderer::ComputeLightViewProjection() const
    {
        const glm::vec3 toSun = glm::normalize(m_lighting.sunDirection);
        const glm::vec3 up = std::abs(toSun.y) > 0.99f
            ? glm::vec3{ 0.0f, 0.0f, 1.0f }
            : glm::vec3{ 0.0f, 1.0f, 0.0f };

        // Fixed orientation, origin at the world origin: only the ortho box
        // moves, so snapping it to whole texels keeps the rasterisation of
        // static geometry identical from frame to frame.
        const glm::mat4 lightView =
            glm::lookAt(glm::vec3{ 0.0f }, -toSun, up);

        const glm::vec3 cameraPosition{ glm::inverse(m_camera.view)[3] };
        glm::vec3 center{ lightView * glm::vec4{ cameraPosition, 1.0f } };

        const float halfExtent = m_lighting.shadowHalfExtent;
        const float texel = 2.0f * halfExtent / static_cast<float>(ShadowMapSize);
        center.x = std::floor(center.x / texel) * texel;
        center.y = std::floor(center.y / texel) * texel;

        // Deep enough to catch roofs and poles well above/below the camera.
        constexpr float DepthRange = 150.0f;
        const glm::mat4 projection = glm::ortho(
            center.x - halfExtent,
            center.x + halfExtent,
            center.y - halfExtent,
            center.y + halfExtent,
            -center.z - DepthRange,
            -center.z + DepthRange
        );

        return projection * lightView;
    }

    std::uint32_t Renderer::DrawQueue(
        SDL_GPURenderPass* renderPass,
        SDL_GPUCommandBuffer* commandBuffer,
        const glm::mat4& viewProjection,
        std::uint32_t sceneSamples,
        bool spotPass
    )
    {
        const Frustum frustum = ExtractFrustum(viewProjection);
        const bool bindMaterials = sceneSamples > 0;
        SDL_GPUGraphicsPipeline* bound = nullptr;
        const Material* boundMaterial = nullptr;

        // One box test per chunk for this view; the shadow pass drops chunks
        // that cast no shadow (skyline, mid-distance shells).
        std::vector<char> chunkVisible(m_chunks.size());
        for (std::size_t i = 0; i < m_chunks.size(); ++i)
        {
            const ChunkInfo& chunk = m_chunks[i];
            chunkVisible[i] = (bindMaterials || chunk.castsShadow)
                && IsBoxVisible(frustum, chunk.boundsMin, chunk.boundsMax);
            if (bindMaterials && chunkVisible[i])
            {
                ++m_stats.layers[static_cast<std::size_t>(chunk.layer)].chunksVisible;
            }
        }

        ObjectUniforms uniforms{};
        uniforms.viewProjection = viewProjection;

        // Light culling (M46), scene pass only: which lights can reach each
        // draw. The spot's shadow frustum is exactly its volume of light.
        const bool spotOn = bindMaterials && m_spotActive;
        const Frustum spotFrustum = ExtractFrustum(spotOn ? SpotMath::ViewProjection(m_spot) : glm::mat4{ 1.0f });

        std::uint32_t drawn = 0;
        // Phase 0: opaque and alpha-tested. Phase 1 (scene only): decals,
        // over the finished surfaces; they cast no shadows.
        for (int phase = 0; phase < (bindMaterials ? 2 : 1); ++phase)
        for (const DrawCommand& command : m_drawCommands)
        {
            const bool isDecal = command.material->alphaMode == AlphaMode::Blend;
            if (isDecal != (phase == 1))
            {
                continue;
            }
            if (command.chunk >= 0 && !chunkVisible[command.chunk])
            {
                continue;
            }
            if (!IsVisible(frustum, *command.mesh, command.model))
            {
                continue;
            }
            ++drawn;
            LayerStats& layer = m_stats.layers[static_cast<std::size_t>(
                command.chunk >= 0 ? m_chunks[command.chunk].layer : RenderLayer::Near)];
            if (bindMaterials)
            {
                ++layer.drawn;
                layer.triangles += command.mesh->GetIndexCount() / 3;
            }
            else
            {
                layer.shadowDrawn += spotPass ? 0 : 1; // the spot's are counted apart
            }

            uniforms.model = command.model;
            SDL_PushGPUVertexUniformData(
                commandBuffer,
                0,
                &uniforms,
                sizeof(uniforms)
            );
            const bool skinned = command.palette >= 0;
            if (skinned)
            {
                const PaletteRange& range = m_paletteRanges[command.palette];
                SDL_PushGPUVertexUniformData(
                    commandBuffer,
                    2,
                    m_palettes.data() + range.first,
                    range.count * static_cast<Uint32>(sizeof(glm::mat4))
                );
            }

            const Material& material = *command.material;
            const bool masked = material.alphaMode == AlphaMode::Mask;
            const float cutoff = masked ? material.alphaCutoff : 0.0f;
            const Texture* baseColor = material.baseColorTexture
                ? material.baseColorTexture
                : m_whiteTexture.get();

            if (bindMaterials)
            {
                SDL_GPUGraphicsPipeline* pipeline = isDecal
                    ? GetDecalPipeline(sceneSamples, skinned)
                    : GetScenePipeline(sceneSamples, material.doubleSided, masked, skinned);
                if (!pipeline)
                {
                    continue;
                }
                if (pipeline != bound)
                {
                    SDL_BindGPUGraphicsPipeline(renderPass, pipeline);
                    bound = pipeline;
                    boundMaterial = nullptr; // rebind textures with a new pipeline
                    ++m_stats.pipelineBinds;
                }

                // Baked meshes blend toward their vertex light; others keep
                // the hemisphere ambient (weight 0).
                const float bakedWeight =
                    command.mesh->HasBakedLight() ? m_lighting.bakedLight : 0.0f;
                // Which lights reach this draw.
                glm::vec3 boxCenter{ 0.0f };
                glm::vec3 boxExtent{ 0.0f };
                WorldBox(*command.mesh, command.model, boxCenter, boxExtent);
                std::uint32_t liveBits = 0;
                for (std::size_t i = 0; i < m_liveLightCount; ++i)
                {
                    if (SpotMath::SphereTouchesBox(m_liveLights[i].position, m_liveLights[i].radius, boxCenter, boxExtent))
                    {
                        liveBits |= 1u << i;
                    }
                }
                const bool spotReaches = spotOn && IsVisible(spotFrustum, *command.mesh, command.model);
                m_stats.spotLitDraws += spotReaches ? 1 : 0;
                m_stats.liveLitDraws += liveBits != 0 ? 1 : 0;

                const MaterialUniforms materialUniforms{
                    material.baseColorFactor,
                    glm::vec4{ material.emissiveFactor, bakedWeight },
                    glm::vec4{
                        material.lightmapIntensity,
                        material.lightmap ? m_lighting.bakedLight : 0.0f,
                        material.wet,
                        skinned && m_skinWeightsView ? 1.0f : 0.0f },
                    glm::vec4{ cutoff, masked && CanUseAlphaToCoverage(sceneSamples) ? 1.0f : 0.0f,
                               material.emissiveTexture ? 1.0f : 0.0f, material.fogAmount },
                    glm::vec4{ SpotMath::Shininess(material.roughness),
                               SpotMath::SpecularStrength(material.roughness, material.specular),
                               material.reveal, 0.0f },
                    glm::vec4{ static_cast<float>(liveBits), spotReaches ? 1.0f : 0.0f, 0.0f, 0.0f }
                };
                SDL_PushGPUFragmentUniformData(
                    commandBuffer,
                    0,
                    &materialUniforms,
                    sizeof(materialUniforms)
                );

                if (baseColor->IsPixelArt()
                    || (material.emissiveTexture && material.emissiveTexture->IsPixelArt()))
                {
                    ++m_stats.renderTextureDraws; // a live screen (M27)
                }

                // Sorted by material: consecutive draws often share textures.
                if (&material != boundMaterial)
                {
                    boundMaterial = &material;
                    ++m_stats.materialBinds;
                    const SDL_GPUTextureSamplerBinding textureBinding{
                        baseColor->GetGPUTexture(),
                        baseColor->IsPixelArt() ? m_pixelSampler : m_sampler
                    };
                    SDL_BindGPUFragmentSamplers(renderPass, 0, &textureBinding, 1);

                    const Texture* lightmap = material.lightmap
                        ? material.lightmap
                        : m_whiteTexture.get();
                    const SDL_GPUTextureSamplerBinding lightmapBinding{
                        lightmap->GetGPUTexture(),
                        m_lightmapSampler
                    };
                    SDL_BindGPUFragmentSamplers(renderPass, 2, &lightmapBinding, 1);

                    const Texture* emissive = material.emissiveTexture
                        ? material.emissiveTexture
                        : m_whiteTexture.get();
                    const SDL_GPUTextureSamplerBinding emissiveBinding{
                        emissive->GetGPUTexture(),
                        emissive->IsPixelArt() ? m_pixelSampler : m_sampler
                    };
                    SDL_BindGPUFragmentSamplers(renderPass, 3, &emissiveBinding, 1);
                }
            }
            else
            {
                SDL_GPUGraphicsPipeline* pipeline = GetShadowPipeline(skinned);
                if (!pipeline)
                {
                    continue;
                }
                if (pipeline != bound)
                {
                    SDL_BindGPUGraphicsPipeline(renderPass, pipeline);
                    bound = pipeline;
                }
                // Depth only: just enough to cut the same holes as the scene.
                const ShadowMaterialUniforms shadowUniforms{
                    glm::vec4{ cutoff, material.baseColorFactor.a, 0.0f, 0.0f }
                };
                SDL_PushGPUFragmentUniformData(
                    commandBuffer, 0, &shadowUniforms, sizeof(shadowUniforms));
                const SDL_GPUTextureSamplerBinding textureBinding{
                    baseColor->GetGPUTexture(),
                    m_sampler
                };
                SDL_BindGPUFragmentSamplers(renderPass, 0, &textureBinding, 1);
            }

            const SDL_GPUBufferBinding vertexBindings[2]{
                { command.mesh->GetVertexBuffer(), 0 },
                { command.mesh->GetSkinBuffer(), 0 }
            };
            const SDL_GPUBufferBinding indexBinding{
                command.mesh->GetIndexBuffer(),
                0
            };
            SDL_BindGPUVertexBuffers(renderPass, 0, vertexBindings, skinned ? 2 : 1);
            SDL_BindGPUIndexBuffer(
                renderPass,
                &indexBinding,
                SDL_GPU_INDEXELEMENTSIZE_32BIT
            );
            SDL_DrawGPUIndexedPrimitives(
                renderPass,
                command.mesh->GetIndexCount(),
                1,
                0,
                0,
                0
            );
        }

        return drawn;
    }

    bool Renderer::RenderShadowPass(
        SDL_GPUCommandBuffer* commandBuffer,
        const glm::mat4& lightViewProjection
    )
    {
        if (!m_lighting.shadowsEnabled)
        {
            return true;
        }

        SDL_GPUDepthStencilTargetInfo depthTarget{};
        depthTarget.texture = m_shadowMap;
        depthTarget.clear_depth = 1.0f;
        depthTarget.load_op = SDL_GPU_LOADOP_CLEAR;
        depthTarget.store_op = SDL_GPU_STOREOP_STORE; // sampled by the scene
        depthTarget.stencil_load_op = SDL_GPU_LOADOP_DONT_CARE;
        depthTarget.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE;
        depthTarget.cycle = true;

        SDL_GPURenderPass* renderPass =
            SDL_BeginGPURenderPass(commandBuffer, nullptr, 0, &depthTarget);
        if (!renderPass)
        {
            std::cerr
                << "Failed to begin shadow render pass: "
                << SDL_GetError()
                << '\n';
            return false;
        }

        SDL_BindGPUGraphicsPipeline(renderPass, m_shadowPipelines[0]);
        SDL_PushGPUVertexUniformData(commandBuffer, 1, &m_wind, sizeof(m_wind));
        m_stats.shadowDrawn =
            DrawQueue(renderPass, commandBuffer, lightViewProjection, 0);

        SDL_EndGPURenderPass(renderPass);
        return true;
    }

    bool Renderer::RenderSpotShadowPass(
        SDL_GPUCommandBuffer* commandBuffer,
        const glm::mat4& spotViewProjection
    )
    {
        if (!m_spotActive || !m_spot.castsShadows)
        {
            return true;
        }

        SDL_GPUDepthStencilTargetInfo depthTarget{};
        depthTarget.texture = m_spotShadowMap;
        depthTarget.clear_depth = 1.0f;
        depthTarget.load_op = SDL_GPU_LOADOP_CLEAR;
        depthTarget.store_op = SDL_GPU_STOREOP_STORE; // sampled by the scene
        depthTarget.stencil_load_op = SDL_GPU_LOADOP_DONT_CARE;
        depthTarget.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE;
        depthTarget.cycle = true;

        SDL_GPURenderPass* renderPass =
            SDL_BeginGPURenderPass(commandBuffer, nullptr, 0, &depthTarget);
        if (!renderPass)
        {
            std::cerr << "Failed to begin spot shadow pass: " << SDL_GetError() << '\n';
            return false;
        }

        // The same depth-only pipelines as the sun's: only the light's
        // matrix differs. DrawQueue culls to that matrix's frustum, so the
        // pass draws only what is inside the cone and within its range.
        SDL_BindGPUGraphicsPipeline(renderPass, m_shadowPipelines[0]);
        SDL_PushGPUVertexUniformData(commandBuffer, 1, &m_wind, sizeof(m_wind));
        m_stats.spotShadowDrawn = DrawQueue(renderPass, commandBuffer, spotViewProjection, 0, true);

        SDL_EndGPURenderPass(renderPass);
        return true;
    }

    bool Renderer::RenderScenePass(
        SDL_GPUCommandBuffer* commandBuffer,
        const glm::mat4& lightViewProjection,
        const glm::mat4& spotViewProjection
    )
    {
        SDL_GPUGraphicsPipeline* pipeline =
            GetScenePipeline(m_targets.GetSamples());
        if (!pipeline)
        {
            return false;
        }

        const SDL_FColor clearColor{
            m_lighting.fogColor.r,
            m_lighting.fogColor.g,
            m_lighting.fogColor.b,
            1.0f
        };
        const SDL_GPUColorTargetInfo colorTarget =
            m_targets.MakeColorTargetInfo(clearColor);
        const SDL_GPUDepthStencilTargetInfo depthTarget =
            m_targets.MakeDepthTargetInfo();

        SDL_GPURenderPass* renderPass = SDL_BeginGPURenderPass(
            commandBuffer,
            &colorTarget,
            1,
            &depthTarget
        );
        if (!renderPass)
        {
            std::cerr
                << "Failed to begin scene render pass: "
                << SDL_GetError()
                << '\n';
            return false;
        }

        const float aspect =
            static_cast<float>(m_targets.GetWidth())
            / static_cast<float>(m_targets.GetHeight());
        const glm::mat4 projection = glm::perspective(
            m_camera.verticalFov,
            aspect,
            m_camera.nearPlane,
            m_camera.farPlane
        );

        SDL_BindGPUGraphicsPipeline(renderPass, pipeline);
        SDL_PushGPUVertexUniformData(commandBuffer, 1, &m_wind, sizeof(m_wind));

        // Once per frame; stays bound for every draw in this command buffer.
        const SceneUniforms sceneUniforms = MakeSceneUniforms(
            m_lighting, m_camera.view, lightViewProjection, m_wind.w,
            std::span<const LiveLight>(m_liveLights.data(), m_liveLightCount),
            m_spotActive ? &m_spot : nullptr, spotViewProjection);
        m_stats.liveLights = static_cast<std::uint32_t>(m_liveLightCount);
        SDL_PushGPUFragmentUniformData(
            commandBuffer,
            1,
            &sceneUniforms,
            sizeof(sceneUniforms)
        );

        const SDL_GPUTextureSamplerBinding shadowBinding{
            m_shadowMap,
            m_shadowSampler
        };
        SDL_BindGPUFragmentSamplers(renderPass, 1, &shadowBinding, 1);
        // The spot's shadow map (M43), same comparison sampler; bound even
        // with no spot (its colour is zero then, so what it holds is unused).
        const SDL_GPUTextureSamplerBinding spotShadowBinding{
            m_spotShadowMap,
            m_shadowSampler
        };
        SDL_BindGPUFragmentSamplers(renderPass, 4, &spotShadowBinding, 1);

        DrawSky(renderPass, commandBuffer, projection);

        m_stats.sceneWidth = m_targets.GetWidth();
        m_stats.sceneHeight = m_targets.GetHeight();
        m_stats.msaaSamples = m_targets.GetSamples();
        m_stats.drawn = DrawQueue(
            renderPass, commandBuffer, projection * m_camera.view, m_targets.GetSamples());

        DrawParticles(renderPass, commandBuffer, projection * m_camera.view);
        DrawBeam(renderPass, commandBuffer, projection * m_camera.view);
        m_spotActive = false; // submitted per frame
        m_liveLightCount = 0;

        SDL_EndGPURenderPass(renderPass);
        return true;
    }

    void Renderer::SubmitParticles(std::span<const Particle> particles)
    {
        m_particles.insert(m_particles.end(), particles.begin(), particles.end());
    }

    void Renderer::RequestCapture(const std::string& path, bool includeUi)
    {
        m_capturePath = path;
        m_captureUi = includeUi;
    }

    bool Renderer::RenderCapture(SDL_GPUCommandBuffer* commandBuffer, std::uint32_t width, std::uint32_t height)
    {
        // The same passes that drew the swapchain image, drawn again into a
        // texture of the swapchain's format that can be copied back.
        if (!m_captureTexture || m_captureWidth != width || m_captureHeight != height)
        {
            if (m_captureTexture)
            {
                SDL_ReleaseGPUTexture(m_device, m_captureTexture);
                SDL_ReleaseGPUTransferBuffer(m_device, m_captureTransfer);
            }
            SDL_GPUTextureCreateInfo info{};
            info.type = SDL_GPU_TEXTURETYPE_2D;
            info.format = SDL_GetGPUSwapchainTextureFormat(m_device, m_window);
            info.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;
            info.width = width;
            info.height = height;
            info.layer_count_or_depth = 1;
            info.num_levels = 1;
            m_captureTexture = SDL_CreateGPUTexture(m_device, &info);
            SDL_GPUTransferBufferCreateInfo transfer{};
            transfer.usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;
            transfer.size = width * height * 4;
            m_captureTransfer = SDL_CreateGPUTransferBuffer(m_device, &transfer);
            m_captureWidth = width;
            m_captureHeight = height;
            if (!m_captureTexture || !m_captureTransfer)
            {
                std::cerr << "Cannot capture: " << SDL_GetError() << '\n';
                m_capturePath.clear();
                return true; // the frame itself is fine
            }
        }
        if (!RenderPostPass(commandBuffer, m_captureTexture, width, height)
            || (m_captureUi && !m_ui.Render(commandBuffer, m_captureTexture, width, height)))
        {
            return false;
        }
        SDL_GPUCopyPass* copy = SDL_BeginGPUCopyPass(commandBuffer);
        SDL_GPUTextureRegion source{};
        source.texture = m_captureTexture;
        source.w = width;
        source.h = height;
        source.d = 1;
        SDL_GPUTextureTransferInfo destination{};
        destination.transfer_buffer = m_captureTransfer;
        destination.pixels_per_row = width;
        destination.rows_per_layer = height;
        SDL_DownloadFromGPUTexture(copy, &source, &destination);
        SDL_EndGPUCopyPass(copy);
        m_captureRecorded = true;
        return true;
    }

    void Renderer::FinishCapture(SDL_GPUFence* fence)
    {
        SDL_WaitForGPUFences(m_device, true, &fence, 1);
        SDL_ReleaseGPUFence(m_device, fence);
        m_captureRecorded = false;

        const auto* mapped = static_cast<const std::uint8_t*>(
            SDL_MapGPUTransferBuffer(m_device, m_captureTransfer, false));
        if (!mapped)
        {
            std::cerr << "Cannot read the capture back: " << SDL_GetError() << '\n';
            m_capturePath.clear();
            return;
        }
        // Swapchains are usually BGRA; PNG wants RGBA, fully opaque.
        const SDL_GPUTextureFormat format = SDL_GetGPUSwapchainTextureFormat(m_device, m_window);
        const bool bgra = format == SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM
            || format == SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB;
        std::vector<std::uint8_t> pixels(static_cast<std::size_t>(m_captureWidth) * m_captureHeight * 4);
        for (std::size_t i = 0; i < pixels.size(); i += 4)
        {
            pixels[i + 0] = mapped[i + (bgra ? 2 : 0)];
            pixels[i + 1] = mapped[i + 1];
            pixels[i + 2] = mapped[i + (bgra ? 0 : 2)];
            pixels[i + 3] = 255;
        }
        SDL_UnmapGPUTransferBuffer(m_device, m_captureTransfer);

        const std::filesystem::path path(m_capturePath);
        m_capturePath.clear();
        if (path.has_parent_path())
        {
            std::error_code error;
            std::filesystem::create_directories(path.parent_path(), error);
        }
        // Unfiltered rows (still compressed): simple for the GIF tool to read
        // back quickly (Tools/Docs/make_gif.py).
        stbi_write_force_png_filter = 0;
        if (!stbi_write_png(path.string().c_str(), static_cast<int>(m_captureWidth), static_cast<int>(m_captureHeight),
                            4, pixels.data(), static_cast<int>(m_captureWidth) * 4))
        {
            std::cerr << "Cannot write " << path.string() << '\n';
            return;
        }
        std::cout << "Captured " << path.string() << '\n';
    }

    std::unique_ptr<RenderTexture> Renderer::CreateRenderTexture(std::uint32_t width, std::uint32_t height)
    {
        std::unique_ptr<RenderTexture> target(new RenderTexture());
        target->m_texture = Texture::CreateRenderTarget(m_device, width, height);
        if (!target->m_texture
            || !target->m_canvas.Initialize(m_device, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB))
        {
            std::cerr << "Failed to create a " << width << "x" << height << " render texture\n";
            return nullptr;
        }
        target->m_owner = this;
        m_renderTextures.push_back(target.get());
        return target;
    }

    void Renderer::UnregisterRenderTexture(RenderTexture* target)
    {
        std::erase(m_renderTextures, target);
    }

    bool Renderer::RenderTextures(SDL_GPUCommandBuffer* commandBuffer)
    {
        for (RenderTexture* target : m_renderTextures)
        {
            if (target->Render(commandBuffer))
            {
                ++m_stats.renderTextures;
            }
        }
        return true;
    }

    void Renderer::SubmitLiveLight(const LiveLight& light)
    {
        // Past the limit, the extra lights are dropped (a level asks for
        // few; the shader loops over what it gets).
        if (m_liveLightCount < m_liveLights.size())
        {
            m_liveLights[m_liveLightCount++] = light;
        }
    }

    void Renderer::SubmitHalos(std::span<const Particle> halos)
    {
        m_halos.insert(m_halos.end(), halos.begin(), halos.end());
    }

    bool Renderer::GlowActive() const
    {
        return m_settings.post.enabled && m_lighting.glowStrength > 0.0f;
    }

    SDL_GPUGraphicsPipeline* Renderer::GetSkyPipeline(std::uint32_t samples)
    {
        const std::size_t slot = SampleSlot(samples);
        if (m_skyPipelines[slot])
        {
            return m_skyPipelines[slot];
        }
        SDL_GPUShader* vertexShader = LoadShader(
            m_device, "Sky.vert", SDL_GPU_SHADERSTAGE_VERTEX, ShaderResources{ .uniformBuffers = 1 });
        SDL_GPUShader* fragmentShader = LoadShader(
            m_device, "Sky.frag", SDL_GPU_SHADERSTAGE_FRAGMENT,
            ShaderResources{ .samplers = 1, .uniformBuffers = 1 });
        if (vertexShader && fragmentShader)
        {
            SDL_GPUColorTargetDescription colorTarget{};
            colorTarget.format = m_targets.GetColorFormat();
            SDL_GPUGraphicsPipelineCreateInfo createInfo{};
            createInfo.vertex_shader = vertexShader;
            createInfo.fragment_shader = fragmentShader;
            createInfo.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
            createInfo.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
            createInfo.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
            createInfo.multisample_state.sample_count = SampleCountFor(slot);
            // On the far plane, drawn first: everything else covers it.
            createInfo.depth_stencil_state.enable_depth_test = true;
            createInfo.depth_stencil_state.enable_depth_write = false;
            createInfo.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS_OR_EQUAL;
            createInfo.target_info.color_target_descriptions = &colorTarget;
            createInfo.target_info.num_color_targets = 1;
            createInfo.target_info.depth_stencil_format = RenderTargets::GetDepthFormat();
            createInfo.target_info.has_depth_stencil_target = true;
            m_skyPipelines[slot] = SDL_CreateGPUGraphicsPipeline(m_device, &createInfo);
            if (!m_skyPipelines[slot])
            {
                std::cerr << "Failed to create sky pipeline: " << SDL_GetError() << '\n';
            }
        }
        if (vertexShader)
        {
            SDL_ReleaseGPUShader(m_device, vertexShader);
        }
        if (fragmentShader)
        {
            SDL_ReleaseGPUShader(m_device, fragmentShader);
        }
        return m_skyPipelines[slot];
    }

    void Renderer::DrawSky(SDL_GPURenderPass* renderPass, SDL_GPUCommandBuffer* commandBuffer,
                           const glm::mat4& projection)
    {
        if (!m_lighting.skyPanorama)
        {
            return; // the pass cleared to the fog colour
        }
        SDL_GPUGraphicsPipeline* pipeline = GetSkyPipeline(m_targets.GetSamples());
        if (!pipeline)
        {
            return;
        }
        // Rotation only: the sky is infinitely far, so moving never changes it.
        glm::mat4 view = m_camera.view;
        view[3] = glm::vec4{ 0.0f, 0.0f, 0.0f, 1.0f };
        const glm::mat4 inverse = glm::inverse(projection * view);
        const glm::vec4 params{ m_lighting.skyIntensity, 0.0f, 0.0f, 0.0f };

        SDL_BindGPUGraphicsPipeline(renderPass, pipeline);
        SDL_PushGPUVertexUniformData(commandBuffer, 0, &inverse, sizeof(inverse));
        SDL_PushGPUFragmentUniformData(commandBuffer, 0, &params, sizeof(params));
        const SDL_GPUTextureSamplerBinding binding{ m_lighting.skyPanorama->GetGPUTexture(), m_sampler };
        SDL_BindGPUFragmentSamplers(renderPass, 0, &binding, 1);
        SDL_DrawGPUPrimitives(renderPass, 3, 1, 0, 0);
    }

    void Renderer::SetParticleAtlas(const Texture* atlas, std::uint32_t columns)
    {
        m_particleAtlas = atlas;
        m_particleAtlasColumns = std::max<std::uint32_t>(1, columns);
    }

    bool Renderer::CreateParticleResources()
    {
        SDL_GPUBufferCreateInfo bufferInfo{};
        bufferInfo.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
        bufferInfo.size = MaxParticles * sizeof(Particle);
        m_particleBuffer = SDL_CreateGPUBuffer(m_device, &bufferInfo);

        SDL_GPUTransferBufferCreateInfo transferInfo{};
        transferInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
        transferInfo.size = MaxParticles * sizeof(Particle);
        m_particleTransfer = SDL_CreateGPUTransferBuffer(m_device, &transferInfo);

        if (!m_particleBuffer || !m_particleTransfer)
        {
            std::cerr
                << "Failed to create particle buffers: "
                << SDL_GetError()
                << '\n';
            return false;
        }
        return true;
    }

    SDL_GPUGraphicsPipeline* Renderer::GetParticlePipeline(std::uint32_t samples, bool additive)
    {
        const std::size_t slot = SampleSlot(samples);
        auto& pipelines = additive ? m_haloPipelines : m_particlePipelines;
        if (pipelines[slot])
        {
            return pipelines[slot];
        }

        SDL_GPUShader* vertexShader = LoadShader(
            m_device,
            "Particle.vert",
            SDL_GPU_SHADERSTAGE_VERTEX,
            ShaderResources{ .uniformBuffers = 1 }
        );
        SDL_GPUShader* fragmentShader = LoadShader(
            m_device,
            additive ? "Halo.frag" : "Particle.frag",
            SDL_GPU_SHADERSTAGE_FRAGMENT,
            // Slot 1 carries SceneUniforms (fog), shared with Basic.frag.
            ShaderResources{ .samplers = 1, .uniformBuffers = 2 }
        );

        if (!vertexShader || !fragmentShader)
        {
            if (vertexShader)
            {
                SDL_ReleaseGPUShader(m_device, vertexShader);
            }
            if (fragmentShader)
            {
                SDL_ReleaseGPUShader(m_device, fragmentShader);
            }
            return nullptr;
        }

        SDL_GPUVertexBufferDescription instanceBuffer{};
        instanceBuffer.slot = 0;
        instanceBuffer.pitch = sizeof(Particle);
        instanceBuffer.input_rate = SDL_GPU_VERTEXINPUTRATE_INSTANCE;

        SDL_GPUVertexAttribute attributes[3]{};
        for (Uint32 i = 0; i < 3; ++i)
        {
            attributes[i].location = i;
            attributes[i].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
            attributes[i].offset = i * 16;
        }

        // Straight alpha blending over the opaque scene.
        SDL_GPUColorTargetDescription colorTarget{};
        colorTarget.format = m_targets.GetColorFormat();
        colorTarget.blend_state.enable_blend = true;
        colorTarget.blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
        colorTarget.blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
        colorTarget.blend_state.color_blend_op = SDL_GPU_BLENDOP_ADD;
        colorTarget.blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
        colorTarget.blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
        colorTarget.blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
        if (additive)
        {
            // Light adds: colour * alpha + what's there.
            colorTarget.blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
            colorTarget.blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
        }

        SDL_GPUGraphicsPipelineCreateInfo createInfo{};
        createInfo.vertex_shader = vertexShader;
        createInfo.fragment_shader = fragmentShader;
        createInfo.vertex_input_state.vertex_buffer_descriptions = &instanceBuffer;
        createInfo.vertex_input_state.num_vertex_buffers = 1;
        createInfo.vertex_input_state.vertex_attributes = attributes;
        createInfo.vertex_input_state.num_vertex_attributes = 3;
        createInfo.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
        createInfo.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
        createInfo.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
        createInfo.rasterizer_state.enable_depth_clip = true;
        createInfo.multisample_state.sample_count = SampleCountFor(slot);
        // Tested against the scene but not written: particles overlap freely.
        createInfo.depth_stencil_state.enable_depth_test = true;
        createInfo.depth_stencil_state.enable_depth_write = false;
        createInfo.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS_OR_EQUAL;
        createInfo.target_info.color_target_descriptions = &colorTarget;
        createInfo.target_info.num_color_targets = 1;
        createInfo.target_info.depth_stencil_format = RenderTargets::GetDepthFormat();
        createInfo.target_info.has_depth_stencil_target = true;

        SDL_GPUGraphicsPipeline* pipeline =
            SDL_CreateGPUGraphicsPipeline(m_device, &createInfo);

        SDL_ReleaseGPUShader(m_device, vertexShader);
        SDL_ReleaseGPUShader(m_device, fragmentShader);

        if (!pipeline)
        {
            std::cerr
                << "Failed to create particle pipeline: "
                << SDL_GetError()
                << '\n';
            return nullptr;
        }

        // Cached in its own array: halos and particles are two pipelines.
        // (Until v0.0.7 this stored every pipeline as the particles' - so
        // particles drew with the halo shader, and a halo pipeline was
        // created, and leaked, every frame.)
        pipelines[slot] = pipeline;
        return pipeline;
    }

    bool Renderer::UploadParticles(SDL_GPUCommandBuffer* commandBuffer)
    {
        m_uploadedParticles = 0;
        m_uploadedHalos = 0;
        if ((m_particles.empty() && m_halos.empty()) || !m_particleAtlas)
        {
            m_halos.clear();
            return true;
        }

        // Back to front, so alpha blending composites correctly.
        const glm::vec3 cameraPosition{ glm::inverse(m_camera.view)[3] };
        std::sort(
            m_particles.begin(),
            m_particles.end(),
            [&](const Particle& a, const Particle& b) {
                const glm::vec3 da = a.position - cameraPosition;
                const glm::vec3 db = b.position - cameraPosition;
                return glm::dot(da, da) > glm::dot(db, db);
            }
        );

        // Halos follow the particles in the same instance buffer; additive,
        // so they need no sorting.
        const auto count = static_cast<std::uint32_t>(
            std::min<std::size_t>(m_particles.size(), MaxParticles));
        const auto halos = static_cast<std::uint32_t>(
            std::min<std::size_t>(m_halos.size(), MaxParticles - count));
        const Uint32 bytes = (count + halos) * sizeof(Particle);

        // Cycling hands us a fresh buffer if last frame's copy is in flight.
        void* mapped = SDL_MapGPUTransferBuffer(m_device, m_particleTransfer, true);
        if (!mapped)
        {
            std::cerr
                << "Failed to map particle transfer buffer: "
                << SDL_GetError()
                << '\n';
            return false;
        }
        std::memcpy(mapped, m_particles.data(), count * sizeof(Particle));
        std::memcpy(static_cast<Particle*>(mapped) + count, m_halos.data(), halos * sizeof(Particle));
        SDL_UnmapGPUTransferBuffer(m_device, m_particleTransfer);
        m_halos.clear();

        SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(commandBuffer);
        SDL_GPUTransferBufferLocation source{};
        source.transfer_buffer = m_particleTransfer;
        SDL_GPUBufferRegion destination{};
        destination.buffer = m_particleBuffer;
        destination.size = bytes;
        SDL_UploadToGPUBuffer(copyPass, &source, &destination, true);
        SDL_EndGPUCopyPass(copyPass);

        m_uploadedParticles = count;
        m_uploadedHalos = halos;
        return true;
    }

    bool Renderer::CreateBeamResources()
    {
        // Three triangles, each in a plane containing the axis (local +Z),
        // turned 60 degrees apart: apex at the lamp, the far edge at z = 1,
        // half as wide as it is long (scaled to the cone each frame).
        std::vector<Vertex> vertices;
        std::vector<std::uint32_t> indices;
        for (int i = 0; i < 3; ++i)
        {
            const float angle = glm::radians(60.0f * static_cast<float>(i));
            const glm::vec3 across{ std::cos(angle), std::sin(angle), 0.0f };
            const glm::vec3 normal{ -std::sin(angle), std::cos(angle), 0.0f };
            const auto base = static_cast<std::uint32_t>(vertices.size());
            for (const glm::vec3& position : { glm::vec3{ 0.0f }, glm::vec3{ 0.0f, 0.0f, 1.0f } - across,
                                               glm::vec3{ 0.0f, 0.0f, 1.0f } + across })
            {
                Vertex vertex{};
                vertex.position = position;
                vertex.normal = normal;
                vertices.push_back(vertex);
            }
            indices.insert(indices.end(), { base, base + 1, base + 2 });
        }
        m_beamMesh = Mesh::Create(m_device, vertices, indices);
        return m_beamMesh != nullptr;
    }

    SDL_GPUGraphicsPipeline* Renderer::GetBeamPipeline(std::uint32_t samples)
    {
        const std::size_t slot = SampleSlot(samples);
        if (m_beamPipelines[slot])
        {
            return m_beamPipelines[slot];
        }
        SDL_GPUShader* vertexShader = LoadShader(
            m_device, "Beam.vert", SDL_GPU_SHADERSTAGE_VERTEX, ShaderResources{ .uniformBuffers = 1 });
        // Slot 1 carries SceneUniforms: the spot itself, the camera, fog.
        SDL_GPUShader* fragmentShader = LoadShader(
            m_device, "Beam.frag", SDL_GPU_SHADERSTAGE_FRAGMENT, ShaderResources{ .uniformBuffers = 2 });
        if (!vertexShader || !fragmentShader)
        {
            if (vertexShader)
            {
                SDL_ReleaseGPUShader(m_device, vertexShader);
            }
            if (fragmentShader)
            {
                SDL_ReleaseGPUShader(m_device, fragmentShader);
            }
            return nullptr;
        }

        SDL_GPUVertexBufferDescription vertexBuffer{};
        vertexBuffer.slot = 0;
        vertexBuffer.pitch = sizeof(Vertex);
        vertexBuffer.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
        SDL_GPUVertexAttribute attributes[2]{};
        attributes[0].location = 0;
        attributes[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
        attributes[0].offset = offsetof(Vertex, position);
        attributes[1].location = 1;
        attributes[1].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
        attributes[1].offset = offsetof(Vertex, normal);

        // Light adds: colour + what's there.
        SDL_GPUColorTargetDescription colorTarget{};
        colorTarget.format = m_targets.GetColorFormat();
        colorTarget.blend_state.enable_blend = true;
        colorTarget.blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
        colorTarget.blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
        colorTarget.blend_state.color_blend_op = SDL_GPU_BLENDOP_ADD;
        colorTarget.blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ZERO;
        colorTarget.blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
        colorTarget.blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;

        SDL_GPUGraphicsPipelineCreateInfo createInfo{};
        createInfo.vertex_shader = vertexShader;
        createInfo.fragment_shader = fragmentShader;
        createInfo.vertex_input_state.vertex_buffer_descriptions = &vertexBuffer;
        createInfo.vertex_input_state.num_vertex_buffers = 1;
        createInfo.vertex_input_state.vertex_attributes = attributes;
        createInfo.vertex_input_state.num_vertex_attributes = 2;
        createInfo.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
        createInfo.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
        createInfo.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE; // seen from either side
        createInfo.rasterizer_state.enable_depth_clip = true;
        createInfo.multisample_state.sample_count = SampleCountFor(slot);
        // Behind a wall it's hidden; it never hides anything itself.
        createInfo.depth_stencil_state.enable_depth_test = true;
        createInfo.depth_stencil_state.enable_depth_write = false;
        createInfo.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS_OR_EQUAL;
        createInfo.target_info.color_target_descriptions = &colorTarget;
        createInfo.target_info.num_color_targets = 1;
        createInfo.target_info.depth_stencil_format = RenderTargets::GetDepthFormat();
        createInfo.target_info.has_depth_stencil_target = true;

        m_beamPipelines[slot] = SDL_CreateGPUGraphicsPipeline(m_device, &createInfo);
        SDL_ReleaseGPUShader(m_device, vertexShader);
        SDL_ReleaseGPUShader(m_device, fragmentShader);
        if (!m_beamPipelines[slot])
        {
            std::cerr << "Failed to create beam pipeline: " << SDL_GetError() << '\n';
        }
        return m_beamPipelines[slot];
    }

    void Renderer::DrawBeam(SDL_GPURenderPass* renderPass, SDL_GPUCommandBuffer* commandBuffer,
                            const glm::mat4& viewProjection)
    {
        if (!m_spotActive || m_spot.beam <= 0.0f || !m_beamMesh)
        {
            return;
        }
        SDL_GPUGraphicsPipeline* pipeline = GetBeamPipeline(m_targets.GetSamples());
        if (!pipeline)
        {
            return;
        }

        // The planes reach most of the way to the range (the falloff has
        // nearly ended there) and open to the outer cone.
        const glm::vec3 forward = glm::normalize(m_spot.direction);
        const glm::vec3 up = std::abs(forward.y) > 0.99f ? glm::vec3{ 1.0f, 0.0f, 0.0f } : glm::vec3{ 0.0f, 1.0f, 0.0f };
        const glm::vec3 right = glm::normalize(glm::cross(up, forward));
        const glm::vec3 realUp = glm::cross(forward, right);
        const float length = m_spot.range * 0.75f;
        const float halfWidth = length * std::tan(glm::radians(m_spot.outerAngleDegrees));
        glm::mat4 model{ 1.0f };
        model[0] = glm::vec4{ right * halfWidth, 0.0f };
        model[1] = glm::vec4{ realUp * halfWidth, 0.0f };
        model[2] = glm::vec4{ forward * length, 0.0f };
        model[3] = glm::vec4{ m_spot.position, 1.0f };

        const ObjectUniforms object{ viewProjection, model };
        const glm::vec4 beam{ m_spot.beam, 1.2f, 0.0f, 0.0f }; // fades in over the first 1.2 m
        SDL_BindGPUGraphicsPipeline(renderPass, pipeline);
        SDL_PushGPUVertexUniformData(commandBuffer, 0, &object, sizeof(object));
        SDL_PushGPUFragmentUniformData(commandBuffer, 0, &beam, sizeof(beam));
        const SDL_GPUBufferBinding vertices{ m_beamMesh->GetVertexBuffer(), 0 };
        const SDL_GPUBufferBinding indices{ m_beamMesh->GetIndexBuffer(), 0 };
        SDL_BindGPUVertexBuffers(renderPass, 0, &vertices, 1);
        SDL_BindGPUIndexBuffer(renderPass, &indices, SDL_GPU_INDEXELEMENTSIZE_32BIT);
        SDL_DrawGPUIndexedPrimitives(renderPass, m_beamMesh->GetIndexCount(), 1, 0, 0, 0);
    }

    void Renderer::DrawParticles(
        SDL_GPURenderPass* renderPass,
        SDL_GPUCommandBuffer* commandBuffer,
        const glm::mat4& viewProjection
    )
    {
        m_stats.particles = m_uploadedParticles + m_uploadedHalos;
        if (m_uploadedParticles + m_uploadedHalos == 0)
        {
            return;
        }

        SDL_GPUGraphicsPipeline* pipeline =
            GetParticlePipeline(m_targets.GetSamples());
        SDL_GPUGraphicsPipeline* haloPipeline =
            GetParticlePipeline(m_targets.GetSamples(), true);
        if (!pipeline || !haloPipeline)
        {
            return;
        }

        // Billboards face the camera: expand along its right and up axes.
        const glm::mat4 cameraWorld = glm::inverse(m_camera.view);
        const ParticleUniforms uniforms{
            viewProjection,
            glm::vec4{ glm::vec3(cameraWorld[0]), static_cast<float>(m_particleAtlasColumns) },
            glm::vec4{ glm::vec3(cameraWorld[1]), 0.0f }
        };

        SDL_BindGPUGraphicsPipeline(renderPass, pipeline);
        SDL_PushGPUVertexUniformData(commandBuffer, 0, &uniforms, sizeof(uniforms));

        const SDL_GPUTextureSamplerBinding atlasBinding{
            m_particleAtlas->GetGPUTexture(),
            m_postSampler // linear, clamped
        };
        SDL_BindGPUFragmentSamplers(renderPass, 0, &atlasBinding, 1);

        const SDL_GPUBufferBinding instances{ m_particleBuffer, 0 };
        SDL_BindGPUVertexBuffers(renderPass, 0, &instances, 1);
        if (m_uploadedParticles > 0)
        {
            SDL_DrawGPUPrimitives(renderPass, 6, m_uploadedParticles, 0, 0);
        }
        if (m_uploadedHalos > 0)
        {
            // Same billboards, instances after the particles, added on.
            SDL_BindGPUGraphicsPipeline(renderPass, haloPipeline);
            SDL_PushGPUVertexUniformData(commandBuffer, 0, &uniforms, sizeof(uniforms));
            SDL_BindGPUFragmentSamplers(renderPass, 0, &atlasBinding, 1);
            SDL_BindGPUVertexBuffers(renderPass, 0, &instances, 1);
            SDL_DrawGPUPrimitives(renderPass, 6, m_uploadedHalos, 0, m_uploadedParticles);
        }
    }

    bool Renderer::RenderPostPass(
        SDL_GPUCommandBuffer* commandBuffer,
        SDL_GPUTexture* swapchainTexture,
        std::uint32_t outputWidth,
        std::uint32_t outputHeight
    )
    {
        SDL_GPUColorTargetInfo colorTarget{};
        colorTarget.texture = swapchainTexture;
        colorTarget.load_op = SDL_GPU_LOADOP_DONT_CARE; // fully overwritten
        colorTarget.store_op = SDL_GPU_STOREOP_STORE;

        SDL_GPURenderPass* renderPass = SDL_BeginGPURenderPass(
            commandBuffer,
            &colorTarget,
            1,
            nullptr
        );
        if (!renderPass)
        {
            std::cerr
                << "Failed to begin post render pass: "
                << SDL_GetError()
                << '\n';
            return false;
        }

        SDL_BindGPUGraphicsPipeline(renderPass, m_postPipeline);

        const PostSettings& post = m_settings.post;
        const PostUniforms postUniforms{
            glm::vec4{ post.tint, post.enabled ? 1.0f : 0.0f },
            glm::vec4{ post.exposure, post.saturation, post.grain, post.vignette },
            glm::vec4{
                static_cast<float>(outputWidth),
                static_cast<float>(outputHeight),
                // Wrapped so the float keeps integer precision.
                static_cast<float>(m_frameIndex % 4096),
                std::clamp(m_fade, 0.0f, 1.0f)
            },
            // Slightly unstable, like the glow of old hardware and film:
            // a few percent of slow shimmer.
            glm::vec4{
                GlowActive()
                    ? m_lighting.glowStrength * (1.0f + 0.04f * std::sin(m_frameIndex * 0.071f)
                                                      + 0.02f * std::sin(m_frameIndex * 0.23f))
                    : 0.0f,
                0.0f, 0.0f, 0.0f }
        };
        SDL_PushGPUFragmentUniformData(
            commandBuffer,
            0,
            &postUniforms,
            sizeof(postUniforms)
        );

        const SDL_GPUTextureSamplerBinding bindings[2]{
            { m_targets.GetSceneTexture(), m_postSampler },
            { m_glow.GetTexture(), m_postSampler },
        };
        SDL_BindGPUFragmentSamplers(renderPass, 0, bindings, 2);
        SDL_DrawGPUPrimitives(renderPass, 3, 1, 0, 0);

        SDL_EndGPURenderPass(renderPass);
        return true;
    }

    void Renderer::Shutdown()
    {
        m_drawCommands.clear();

        m_beamMesh.reset(); // before the device it lives on
        if (m_device)
        {
            m_ui.Shutdown();
            m_glow.Release();
            for (auto* pipelines : { &m_haloPipelines, &m_skyPipelines, &m_beamPipelines })
            {
                for (SDL_GPUGraphicsPipeline*& pipeline : *pipelines)
                {
                    if (pipeline)
                    {
                        SDL_ReleaseGPUGraphicsPipeline(m_device, pipeline);
                        pipeline = nullptr;
                    }
                }
            }
            m_whiteTexture.reset();
            m_targets.Release();
            if (m_captureTexture)
            {
                SDL_ReleaseGPUTexture(m_device, m_captureTexture);
                SDL_ReleaseGPUTransferBuffer(m_device, m_captureTransfer);
                m_captureTexture = nullptr;
                m_captureTransfer = nullptr;
            }

            for (SDL_GPUSampler* sampler : { m_sampler, m_postSampler, m_lightmapSampler, m_pixelSampler })
            {
                if (sampler)
                {
                    SDL_ReleaseGPUSampler(m_device, sampler);
                }
            }
            for (SDL_GPUGraphicsPipeline* pipeline : m_decalPipelines)
            {
                if (pipeline)
                {
                    SDL_ReleaseGPUGraphicsPipeline(m_device, pipeline);
                }
            }
            for (SDL_GPUGraphicsPipeline* pipeline : m_scenePipelines)
            {
                if (pipeline)
                {
                    SDL_ReleaseGPUGraphicsPipeline(m_device, pipeline);
                }
            }
            if (m_postPipeline)
            {
                SDL_ReleaseGPUGraphicsPipeline(m_device, m_postPipeline);
            }
            for (SDL_GPUGraphicsPipeline* pipeline : m_shadowPipelines)
            {
                if (pipeline)
                {
                    SDL_ReleaseGPUGraphicsPipeline(m_device, pipeline);
                }
            }
            for (SDL_GPUGraphicsPipeline* pipeline : m_particlePipelines)
            {
                if (pipeline)
                {
                    SDL_ReleaseGPUGraphicsPipeline(m_device, pipeline);
                }
            }
            if (m_particleBuffer)
            {
                SDL_ReleaseGPUBuffer(m_device, m_particleBuffer);
            }
            if (m_particleTransfer)
            {
                SDL_ReleaseGPUTransferBuffer(m_device, m_particleTransfer);
            }
            if (m_shadowSampler)
            {
                SDL_ReleaseGPUSampler(m_device, m_shadowSampler);
            }
            if (m_shadowMap)
            {
                SDL_ReleaseGPUTexture(m_device, m_shadowMap);
            }
            if (m_spotShadowMap)
            {
                SDL_ReleaseGPUTexture(m_device, m_spotShadowMap);
            }

            if (m_windowClaimed && m_window)
            {
                SDL_ReleaseWindowFromGPUDevice(m_device, m_window);
            }

            SDL_DestroyGPUDevice(m_device);
        }

        m_scenePipelines = {};
        m_decalPipelines = {};
        m_postPipeline = nullptr;
        m_shadowPipelines = {};
        m_particlePipelines = {};
        m_beamPipelines = {};
        m_particleBuffer = nullptr;
        m_particleTransfer = nullptr;
        m_particles.clear();
        m_particleAtlas = nullptr;
        m_shadowSampler = nullptr;
        m_shadowMap = nullptr;
        m_spotShadowMap = nullptr;
        m_sampler = nullptr;
        m_postSampler = nullptr;
        m_lightmapSampler = nullptr;
        m_pixelSampler = nullptr;
        m_device = nullptr;
        m_window = nullptr;
        m_windowClaimed = false;
    }
}
