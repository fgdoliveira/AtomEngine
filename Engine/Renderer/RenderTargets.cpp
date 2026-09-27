#include "Renderer/RenderTargets.h"

#include <SDL3/SDL.h>

#include <iostream>

namespace Atom
{
    namespace
    {
        SDL_GPUSampleCount ToSampleCount(std::uint32_t samples)
        {
            switch (samples)
            {
            case 4: return SDL_GPU_SAMPLECOUNT_4;
            case 2: return SDL_GPU_SAMPLECOUNT_2;
            default: return SDL_GPU_SAMPLECOUNT_1;
            }
        }

        SDL_GPUTexture* CreateTarget(
            SDL_GPUDevice* device,
            SDL_GPUTextureFormat format,
            SDL_GPUTextureUsageFlags usage,
            std::uint32_t width,
            std::uint32_t height,
            std::uint32_t samples
        )
        {
            SDL_GPUTextureCreateInfo createInfo{};
            createInfo.type = SDL_GPU_TEXTURETYPE_2D;
            createInfo.format = format;
            createInfo.usage = usage;
            createInfo.width = width;
            createInfo.height = height;
            createInfo.layer_count_or_depth = 1;
            createInfo.num_levels = 1;
            createInfo.sample_count = ToSampleCount(samples);

            SDL_GPUTexture* texture = SDL_CreateGPUTexture(device, &createInfo);
            if (!texture)
            {
                std::cerr
                    << "Failed to create render target: "
                    << SDL_GetError()
                    << '\n';
            }
            return texture;
        }
    }

    SDL_GPUTextureFormat RenderTargets::GetDepthFormat()
    {
        return SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
    }

    bool RenderTargets::Initialize(SDL_GPUDevice* device)
    {
        m_device = device;

        // Packed float is half the bandwidth of RGBA16F and the scene needs
        // no alpha; keep RGBA16F as the fallback.
        constexpr SDL_GPUTextureUsageFlags usage =
            SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;
        for (const SDL_GPUTextureFormat format : {
            SDL_GPU_TEXTUREFORMAT_R11G11B10_UFLOAT,
            SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT })
        {
            if (SDL_GPUTextureSupportsFormat(
                device, format, SDL_GPU_TEXTURETYPE_2D, usage))
            {
                m_colorFormat = format;
                break;
            }
        }

        if (m_colorFormat == SDL_GPU_TEXTUREFORMAT_INVALID)
        {
            std::cerr << "GPU supports no HDR scene colour format.\n";
            return false;
        }

        if (!SDL_GPUTextureSupportsFormat(
            device,
            GetDepthFormat(),
            SDL_GPU_TEXTURETYPE_2D,
            SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET))
        {
            std::cerr << "GPU does not support a D32 depth target.\n";
            return false;
        }

        m_maxSamples = 1;
        for (const std::uint32_t samples : { 2u, 4u })
        {
            const SDL_GPUSampleCount count = ToSampleCount(samples);
            if (SDL_GPUTextureSupportsSampleCount(device, m_colorFormat, count)
                && SDL_GPUTextureSupportsSampleCount(device, GetDepthFormat(), count))
            {
                m_maxSamples = samples;
            }
        }

        std::cout
            << "Scene target: format="
            << (m_colorFormat == SDL_GPU_TEXTUREFORMAT_R11G11B10_UFLOAT
                ? "R11G11B10_UFLOAT"
                : "R16G16B16A16_FLOAT")
            << " maxMSAA=" << m_maxSamples << "x\n";
        return true;
    }

    std::uint32_t RenderTargets::ClampSampleCount(std::uint32_t requested) const
    {
        std::uint32_t samples = 1;
        while (samples * 2 <= requested && samples * 2 <= m_maxSamples)
        {
            samples *= 2;
        }
        return samples;
    }

    bool RenderTargets::Ensure(
        std::uint32_t width,
        std::uint32_t height,
        std::uint32_t samples
    )
    {
        samples = ClampSampleCount(samples);
        if (m_resolved && m_width == width && m_height == height
            && m_samples == samples)
        {
            return true;
        }

        ReleaseTextures();

        m_resolved = CreateTarget(
            m_device,
            m_colorFormat,
            SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER,
            width,
            height,
            1
        );
        if (samples > 1)
        {
            m_multisampled = CreateTarget(
                m_device,
                m_colorFormat,
                SDL_GPU_TEXTUREUSAGE_COLOR_TARGET,
                width,
                height,
                samples
            );
        }
        m_depth = CreateTarget(
            m_device,
            GetDepthFormat(),
            SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET,
            width,
            height,
            samples
        );

        if (!m_resolved || !m_depth || (samples > 1 && !m_multisampled))
        {
            ReleaseTextures();
            return false;
        }

        m_width = width;
        m_height = height;
        m_samples = samples;
        return true;
    }

    SDL_GPUColorTargetInfo RenderTargets::MakeColorTargetInfo(
        const SDL_FColor& clear
    ) const
    {
        SDL_GPUColorTargetInfo info{};
        info.clear_color = clear;
        info.load_op = SDL_GPU_LOADOP_CLEAR;
        info.cycle = true;

        if (m_multisampled)
        {
            // Resolve on the tile; the multisampled contents are dropped.
            info.texture = m_multisampled;
            info.store_op = SDL_GPU_STOREOP_RESOLVE;
            info.resolve_texture = m_resolved;
            info.cycle_resolve_texture = true;
        }
        else
        {
            info.texture = m_resolved;
            info.store_op = SDL_GPU_STOREOP_STORE;
        }
        return info;
    }

    SDL_GPUDepthStencilTargetInfo RenderTargets::MakeDepthTargetInfo() const
    {
        SDL_GPUDepthStencilTargetInfo info{};
        info.texture = m_depth;
        info.clear_depth = 1.0f;
        info.load_op = SDL_GPU_LOADOP_CLEAR;
        info.store_op = SDL_GPU_STOREOP_DONT_CARE;
        info.stencil_load_op = SDL_GPU_LOADOP_DONT_CARE;
        info.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE;
        info.cycle = true;
        return info;
    }

    void RenderTargets::ReleaseTextures()
    {
        for (SDL_GPUTexture** texture : { &m_multisampled, &m_resolved, &m_depth })
        {
            if (*texture)
            {
                SDL_ReleaseGPUTexture(m_device, *texture);
                *texture = nullptr;
            }
        }
        m_width = 0;
        m_height = 0;
        m_samples = 0;
    }

    void RenderTargets::Release()
    {
        if (m_device)
        {
            ReleaseTextures();
        }
        m_device = nullptr;
    }
}
