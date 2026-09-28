#include "Renderer/Glow.h"

#include "Renderer/Shader.h"

#include <glm/vec4.hpp>

#include <algorithm>
#include <iostream>

namespace Atom
{
    namespace
    {
        SDL_GPUGraphicsPipeline* CreatePipeline(SDL_GPUDevice* device, SDL_GPUTextureFormat format,
                                                const char* fragment)
        {
            SDL_GPUShader* vertexShader = LoadShader(
                device, "Fullscreen.vert", SDL_GPU_SHADERSTAGE_VERTEX, ShaderResources{});
            SDL_GPUShader* fragmentShader = LoadShader(
                device, fragment, SDL_GPU_SHADERSTAGE_FRAGMENT,
                ShaderResources{ .samplers = 1, .uniformBuffers = 1 });
            SDL_GPUGraphicsPipeline* pipeline = nullptr;
            if (vertexShader && fragmentShader)
            {
                SDL_GPUColorTargetDescription colorTarget{};
                colorTarget.format = format;
                SDL_GPUGraphicsPipelineCreateInfo createInfo{};
                createInfo.vertex_shader = vertexShader;
                createInfo.fragment_shader = fragmentShader;
                createInfo.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
                createInfo.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
                createInfo.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
                createInfo.target_info.color_target_descriptions = &colorTarget;
                createInfo.target_info.num_color_targets = 1;
                pipeline = SDL_CreateGPUGraphicsPipeline(device, &createInfo);
                if (!pipeline)
                {
                    std::cerr << "Failed to create glow pipeline " << fragment << ": " << SDL_GetError() << '\n';
                }
            }
            if (vertexShader)
            {
                SDL_ReleaseGPUShader(device, vertexShader);
            }
            if (fragmentShader)
            {
                SDL_ReleaseGPUShader(device, fragmentShader);
            }
            return pipeline;
        }
    }

    bool Glow::Initialize(SDL_GPUDevice* device, SDL_GPUTextureFormat format)
    {
        m_device = device;
        m_format = format;
        m_brightPipeline = CreatePipeline(device, format, "GlowBright.frag");
        m_blurPipeline = CreatePipeline(device, format, "GlowBlur.frag");

        SDL_GPUSamplerCreateInfo samplerInfo{};
        samplerInfo.min_filter = SDL_GPU_FILTER_LINEAR;
        samplerInfo.mag_filter = SDL_GPU_FILTER_LINEAR;
        samplerInfo.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
        samplerInfo.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        samplerInfo.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        samplerInfo.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        m_sampler = SDL_CreateGPUSampler(device, &samplerInfo);

        // Until the first Render: a tiny black target, so the post pass
        // always has something to sample.
        return m_brightPipeline && m_blurPipeline && m_sampler && EnsureTextures(1, 1);
    }

    void Glow::Release()
    {
        if (!m_device)
        {
            return;
        }
        for (SDL_GPUTexture*& texture : m_textures)
        {
            if (texture)
            {
                SDL_ReleaseGPUTexture(m_device, texture);
                texture = nullptr;
            }
        }
        for (SDL_GPUGraphicsPipeline** pipeline : { &m_brightPipeline, &m_blurPipeline })
        {
            if (*pipeline)
            {
                SDL_ReleaseGPUGraphicsPipeline(m_device, *pipeline);
                *pipeline = nullptr;
            }
        }
        if (m_sampler)
        {
            SDL_ReleaseGPUSampler(m_device, m_sampler);
            m_sampler = nullptr;
        }
        m_device = nullptr;
    }

    bool Glow::EnsureTextures(std::uint32_t width, std::uint32_t height)
    {
        if (m_textures[0] && width == m_width && height == m_height)
        {
            return true;
        }
        for (SDL_GPUTexture*& texture : m_textures)
        {
            if (texture)
            {
                SDL_ReleaseGPUTexture(m_device, texture);
            }
            SDL_GPUTextureCreateInfo info{};
            info.type = SDL_GPU_TEXTURETYPE_2D;
            info.format = m_format;
            info.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;
            info.width = width;
            info.height = height;
            info.layer_count_or_depth = 1;
            info.num_levels = 1;
            texture = SDL_CreateGPUTexture(m_device, &info);
            if (!texture)
            {
                std::cerr << "Failed to create glow target: " << SDL_GetError() << '\n';
                return false;
            }
        }
        m_width = width;
        m_height = height;
        return true;
    }

    bool Glow::Pass(SDL_GPUCommandBuffer* commandBuffer, SDL_GPUGraphicsPipeline* pipeline,
                    SDL_GPUTexture* source, SDL_GPUTexture* target, const void* uniforms,
                    std::uint32_t uniformBytes)
    {
        SDL_GPUColorTargetInfo colorTarget{};
        colorTarget.texture = target;
        colorTarget.load_op = SDL_GPU_LOADOP_DONT_CARE; // fully overwritten
        colorTarget.store_op = SDL_GPU_STOREOP_STORE;
        SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(commandBuffer, &colorTarget, 1, nullptr);
        if (!pass)
        {
            std::cerr << "Failed to begin glow pass: " << SDL_GetError() << '\n';
            return false;
        }
        SDL_BindGPUGraphicsPipeline(pass, pipeline);
        SDL_PushGPUFragmentUniformData(commandBuffer, 0, uniforms, uniformBytes);
        const SDL_GPUTextureSamplerBinding binding{ source, m_sampler };
        SDL_BindGPUFragmentSamplers(pass, 0, &binding, 1);
        SDL_DrawGPUPrimitives(pass, 3, 1, 0, 0);
        SDL_EndGPURenderPass(pass);
        return true;
    }

    bool Glow::Render(SDL_GPUCommandBuffer* commandBuffer, SDL_GPUTexture* scene,
                      std::uint32_t width, std::uint32_t height, float threshold)
    {
        const std::uint32_t w = std::max<std::uint32_t>(1, width / 4);
        const std::uint32_t h = std::max<std::uint32_t>(1, height / 4);
        if (!EnsureTextures(w, h))
        {
            return false;
        }
        // x: threshold; yz: one source texel (for the 4-tap downsample).
        const glm::vec4 bright{ threshold, 1.0f / width, 1.0f / height, 0.0f };
        // xy: one glow texel along the blur direction.
        const glm::vec4 horizontal{ 1.0f / w, 0.0f, 0.0f, 0.0f };
        const glm::vec4 vertical{ 0.0f, 1.0f / h, 0.0f, 0.0f };
        return Pass(commandBuffer, m_brightPipeline, scene, m_textures[0], &bright, sizeof(bright))
            && Pass(commandBuffer, m_blurPipeline, m_textures[0], m_textures[1], &horizontal, sizeof(horizontal))
            && Pass(commandBuffer, m_blurPipeline, m_textures[1], m_textures[0], &vertical, sizeof(vertical));
    }
}
