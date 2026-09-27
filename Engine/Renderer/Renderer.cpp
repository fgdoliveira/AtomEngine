#include "Renderer/Renderer.h"

#include "Renderer/Shader.h"

#include <SDL3/SDL.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/matrix.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <string>

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
            glm::vec4 emissiveFactor;
        };

        struct SceneUniforms
        {
            glm::vec4 sunDirection;
            glm::vec4 sunColor;
            glm::vec4 skyColor;
            glm::vec4 groundColor;
            glm::vec4 fogColor;       // w: density
            glm::vec4 cameraPosition; // w: height falloff
            glm::vec4 fogParams;      // x: base height
        };

        SceneUniforms MakeSceneUniforms(
            const SceneLighting& lighting,
            const glm::mat4& view
        )
        {
            // The camera sits at the translation of the inverse view.
            const glm::vec3 cameraPosition{ glm::inverse(view)[3] };

            SceneUniforms uniforms{};
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
            for (const SDL_GPUPresentMode mode : {
                SDL_GPU_PRESENTMODE_MAILBOX,
                SDL_GPU_PRESENTMODE_IMMEDIATE })
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

        if (!m_sampler || !m_postSampler)
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

    SDL_GPUGraphicsPipeline* Renderer::GetScenePipeline(std::uint32_t samples)
    {
        const std::size_t slot = samples >= 4 ? 2 : samples == 2 ? 1 : 0;
        if (m_scenePipelines[slot])
        {
            return m_scenePipelines[slot];
        }

        SDL_GPUShader* vertexShader = LoadShader(
            m_device,
            "Basic.vert",
            SDL_GPU_SHADERSTAGE_VERTEX,
            ShaderResources{ .uniformBuffers = 1 }
        );
        SDL_GPUShader* fragmentShader = LoadShader(
            m_device,
            "Basic.frag",
            SDL_GPU_SHADERSTAGE_FRAGMENT,
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

        SDL_GPUVertexBufferDescription vertexBuffer{};
        vertexBuffer.slot = 0;
        vertexBuffer.pitch = sizeof(Vertex);
        vertexBuffer.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;

        SDL_GPUVertexAttribute attributes[3]{};
        attributes[0].location = 0;
        attributes[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
        attributes[0].offset = offsetof(Vertex, position);
        attributes[1].location = 1;
        attributes[1].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
        attributes[1].offset = offsetof(Vertex, normal);
        attributes[2].location = 2;
        attributes[2].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
        attributes[2].offset = offsetof(Vertex, uv);

        SDL_GPUColorTargetDescription colorTarget{};
        colorTarget.format = m_targets.GetColorFormat();

        SDL_GPUGraphicsPipelineCreateInfo createInfo{};
        createInfo.vertex_shader = vertexShader;
        createInfo.fragment_shader = fragmentShader;
        createInfo.vertex_input_state.vertex_buffer_descriptions =
            &vertexBuffer;
        createInfo.vertex_input_state.num_vertex_buffers = 1;
        createInfo.vertex_input_state.vertex_attributes = attributes;
        createInfo.vertex_input_state.num_vertex_attributes = 3;
        createInfo.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
        createInfo.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
        createInfo.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_BACK;
        createInfo.rasterizer_state.front_face =
            SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
        createInfo.rasterizer_state.enable_depth_clip = true;
        createInfo.multisample_state.sample_count = slot == 2
            ? SDL_GPU_SAMPLECOUNT_4
            : slot == 1 ? SDL_GPU_SAMPLECOUNT_2 : SDL_GPU_SAMPLECOUNT_1;
        createInfo.depth_stencil_state.enable_depth_test = true;
        createInfo.depth_stencil_state.enable_depth_write = true;
        createInfo.depth_stencil_state.compare_op =
            SDL_GPU_COMPAREOP_LESS_OR_EQUAL;
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

        m_scenePipelines[slot] = pipeline;
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
            ShaderResources{ .samplers = 1 }
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
        std::span<const std::uint32_t> indices
    )
    {
        return Mesh::Create(m_device, vertices, indices);
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
        m_drawCommands.push_back(DrawCommand{ &mesh, &material, model });
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
            ~ClearOnExit() { commands.clear(); }
        } clearDrawCommands{ m_drawCommands };

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

        const bool ok =
            m_targets.Ensure(
                scaled(swapchainWidth),
                scaled(swapchainHeight),
                m_settings.msaaSamples)
            && RenderScenePass(commandBuffer)
            && RenderPostPass(commandBuffer, swapchainTexture);

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

    bool Renderer::RenderScenePass(SDL_GPUCommandBuffer* commandBuffer)
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

        ObjectUniforms uniforms{};
        uniforms.viewProjection = projection * m_camera.view;

        SDL_BindGPUGraphicsPipeline(renderPass, pipeline);

        // Once per frame; stays bound for every draw in this command buffer.
        const SceneUniforms sceneUniforms =
            MakeSceneUniforms(m_lighting, m_camera.view);
        SDL_PushGPUFragmentUniformData(
            commandBuffer,
            1,
            &sceneUniforms,
            sizeof(sceneUniforms)
        );

        const Frustum frustum = ExtractFrustum(uniforms.viewProjection);
        m_stats = FrameStats{};
        m_stats.submitted = static_cast<std::uint32_t>(m_drawCommands.size());
        m_stats.sceneWidth = m_targets.GetWidth();
        m_stats.sceneHeight = m_targets.GetHeight();
        m_stats.msaaSamples = m_targets.GetSamples();

        for (const DrawCommand& command : m_drawCommands)
        {
            if (!IsVisible(frustum, *command.mesh, command.model))
            {
                continue;
            }
            ++m_stats.drawn;

            const SDL_GPUBufferBinding vertexBinding{
                command.mesh->GetVertexBuffer(),
                0
            };
            const SDL_GPUBufferBinding indexBinding{
                command.mesh->GetIndexBuffer(),
                0
            };

            uniforms.model = command.model;
            SDL_PushGPUVertexUniformData(
                commandBuffer,
                0,
                &uniforms,
                sizeof(uniforms)
            );

            const Material& material = *command.material;
            const MaterialUniforms materialUniforms{
                material.baseColorFactor,
                glm::vec4{ material.emissiveFactor, 0.0f }
            };
            SDL_PushGPUFragmentUniformData(
                commandBuffer,
                0,
                &materialUniforms,
                sizeof(materialUniforms)
            );

            const Texture* baseColor = material.baseColorTexture
                ? material.baseColorTexture
                : m_whiteTexture.get();
            const SDL_GPUTextureSamplerBinding textureBinding{
                baseColor->GetGPUTexture(),
                m_sampler
            };
            SDL_BindGPUFragmentSamplers(renderPass, 0, &textureBinding, 1);

            SDL_BindGPUVertexBuffers(renderPass, 0, &vertexBinding, 1);
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

        SDL_EndGPURenderPass(renderPass);
        return true;
    }

    bool Renderer::RenderPostPass(
        SDL_GPUCommandBuffer* commandBuffer,
        SDL_GPUTexture* swapchainTexture
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

        const SDL_GPUTextureSamplerBinding sceneBinding{
            m_targets.GetSceneTexture(),
            m_postSampler
        };
        SDL_BindGPUFragmentSamplers(renderPass, 0, &sceneBinding, 1);
        SDL_DrawGPUPrimitives(renderPass, 3, 1, 0, 0);

        SDL_EndGPURenderPass(renderPass);
        return true;
    }

    void Renderer::Shutdown()
    {
        m_drawCommands.clear();

        if (m_device)
        {
            m_whiteTexture.reset();
            m_targets.Release();

            for (SDL_GPUSampler* sampler : { m_sampler, m_postSampler })
            {
                if (sampler)
                {
                    SDL_ReleaseGPUSampler(m_device, sampler);
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

            if (m_windowClaimed && m_window)
            {
                SDL_ReleaseWindowFromGPUDevice(m_device, m_window);
            }

            SDL_DestroyGPUDevice(m_device);
        }

        m_scenePipelines = {};
        m_postPipeline = nullptr;
        m_sampler = nullptr;
        m_postSampler = nullptr;
        m_device = nullptr;
        m_window = nullptr;
        m_windowClaimed = false;
    }
}
