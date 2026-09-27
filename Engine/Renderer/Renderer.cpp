#include "Renderer/Renderer.h"

#include "Renderer/Shader.h"

#include <SDL3/SDL.h>

#include <glm/gtc/matrix_transform.hpp>

#include <cstddef>
#include <iostream>
#include <string>

namespace Atom
{
    namespace
    {
        constexpr SDL_GPUTextureFormat DepthFormat =
            SDL_GPU_TEXTUREFORMAT_D32_FLOAT;

        // Overcast sky grey; the fog pass will match this later.
        constexpr SDL_FColor ClearColor{ 0.62f, 0.63f, 0.64f, 1.0f };

        // Mirrors the cbuffer in Shaders/Basic.vert.hlsl.
        struct ObjectUniforms
        {
            glm::mat4 viewProjection;
            glm::mat4 model;
        };

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

        if (!SDL_GPUTextureSupportsFormat(
            m_device,
            DepthFormat,
            SDL_GPU_TEXTURETYPE_2D,
            SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET
        ))
        {
            std::cerr << "GPU does not support a D32 depth target.\n";
            return false;
        }

        return CreateBasicPipeline();
    }

    bool Renderer::CreateBasicPipeline()
    {
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
            ShaderResources{}
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
        attributes[2].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
        attributes[2].offset = offsetof(Vertex, color);

        SDL_GPUColorTargetDescription colorTarget{};
        colorTarget.format =
            SDL_GetGPUSwapchainTextureFormat(m_device, m_window);

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
        createInfo.depth_stencil_state.enable_depth_test = true;
        createInfo.depth_stencil_state.enable_depth_write = true;
        createInfo.depth_stencil_state.compare_op =
            SDL_GPU_COMPAREOP_LESS_OR_EQUAL;
        createInfo.target_info.color_target_descriptions = &colorTarget;
        createInfo.target_info.num_color_targets = 1;
        createInfo.target_info.depth_stencil_format = DepthFormat;
        createInfo.target_info.has_depth_stencil_target = true;

        m_basicPipeline = SDL_CreateGPUGraphicsPipeline(m_device, &createInfo);

        // Pipelines keep what they need; the shader objects can go.
        SDL_ReleaseGPUShader(m_device, vertexShader);
        SDL_ReleaseGPUShader(m_device, fragmentShader);

        if (!m_basicPipeline)
        {
            std::cerr
                << "Failed to create basic graphics pipeline: "
                << SDL_GetError()
                << '\n';
            return false;
        }

        return true;
    }

    bool Renderer::EnsureDepthTexture(
        std::uint32_t width,
        std::uint32_t height
    )
    {
        if (m_depthTexture
            && m_depthWidth == width
            && m_depthHeight == height)
        {
            return true;
        }

        if (m_depthTexture)
        {
            SDL_ReleaseGPUTexture(m_device, m_depthTexture);
            m_depthTexture = nullptr;
        }

        SDL_GPUTextureCreateInfo createInfo{};
        createInfo.type = SDL_GPU_TEXTURETYPE_2D;
        createInfo.format = DepthFormat;
        createInfo.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
        createInfo.width = width;
        createInfo.height = height;
        createInfo.layer_count_or_depth = 1;
        createInfo.num_levels = 1;
        createInfo.sample_count = SDL_GPU_SAMPLECOUNT_1;

        m_depthTexture = SDL_CreateGPUTexture(m_device, &createInfo);
        if (!m_depthTexture)
        {
            std::cerr
                << "Failed to create depth texture: "
                << SDL_GetError()
                << '\n';
            m_depthWidth = 0;
            m_depthHeight = 0;
            return false;
        }

        m_depthWidth = width;
        m_depthHeight = height;
        return true;
    }

    std::unique_ptr<Mesh> Renderer::CreateMesh(
        std::span<const Vertex> vertices,
        std::span<const std::uint32_t> indices
    )
    {
        return Mesh::Create(m_device, vertices, indices);
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

    void Renderer::Submit(const Mesh& mesh, const glm::mat4& model)
    {
        m_drawCommands.push_back(DrawCommand{ &mesh, model });
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

        if (!EnsureDepthTexture(swapchainWidth, swapchainHeight))
        {
            SDL_SubmitGPUCommandBuffer(commandBuffer);
            return false;
        }

        SDL_GPUColorTargetInfo colorTarget{};
        colorTarget.texture = swapchainTexture;
        colorTarget.clear_color = ClearColor;
        colorTarget.load_op = SDL_GPU_LOADOP_CLEAR;
        colorTarget.store_op = SDL_GPU_STOREOP_STORE;

        SDL_GPUDepthStencilTargetInfo depthTarget{};
        depthTarget.texture = m_depthTexture;
        depthTarget.clear_depth = 1.0f;
        depthTarget.load_op = SDL_GPU_LOADOP_CLEAR;
        depthTarget.store_op = SDL_GPU_STOREOP_DONT_CARE;
        depthTarget.stencil_load_op = SDL_GPU_LOADOP_DONT_CARE;
        depthTarget.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE;
        depthTarget.cycle = true;

        SDL_GPURenderPass* renderPass = SDL_BeginGPURenderPass(
            commandBuffer,
            &colorTarget,
            1,
            &depthTarget
        );
        if (!renderPass)
        {
            const std::string renderPassError = SDL_GetError();
            const bool submitted = SDL_SubmitGPUCommandBuffer(commandBuffer);
            const std::string submissionError = submitted
                ? ""
                : SDL_GetError();

            std::cerr
                << "Failed to begin GPU render pass: "
                << renderPassError
                << '\n';
            if (!submitted)
            {
                std::cerr
                    << "Failed to submit GPU command buffer during cleanup: "
                    << submissionError
                    << '\n';
            }
            return false;
        }

        const float aspect =
            static_cast<float>(swapchainWidth)
            / static_cast<float>(swapchainHeight);
        const glm::mat4 projection = glm::perspective(
            m_camera.verticalFov,
            aspect,
            m_camera.nearPlane,
            m_camera.farPlane
        );

        ObjectUniforms uniforms{};
        uniforms.viewProjection = projection * m_camera.view;

        SDL_BindGPUGraphicsPipeline(renderPass, m_basicPipeline);

        for (const DrawCommand& command : m_drawCommands)
        {
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

        if (!SDL_SubmitGPUCommandBuffer(commandBuffer))
        {
            std::cerr
                << "Failed to submit GPU command buffer: "
                << SDL_GetError()
                << '\n';
            return false;
        }

        return true;
    }

    void Renderer::Shutdown()
    {
        m_drawCommands.clear();

        if (m_device)
        {
            if (m_depthTexture)
            {
                SDL_ReleaseGPUTexture(m_device, m_depthTexture);
            }
            if (m_basicPipeline)
            {
                SDL_ReleaseGPUGraphicsPipeline(m_device, m_basicPipeline);
            }

            if (m_windowClaimed && m_window)
            {
                SDL_ReleaseWindowFromGPUDevice(m_device, m_window);
            }

            SDL_DestroyGPUDevice(m_device);
        }

        m_depthTexture = nullptr;
        m_depthWidth = 0;
        m_depthHeight = 0;
        m_basicPipeline = nullptr;
        m_device = nullptr;
        m_window = nullptr;
        m_windowClaimed = false;
    }
}
