#include "Renderer/Texture.h"

#include "Renderer/GpuResources.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <bit>
#include <cstring>
#include <iostream>

namespace Atom
{
    std::unique_ptr<Texture> Texture::Create(
        SDL_GPUDevice* device,
        std::uint32_t width,
        std::uint32_t height,
        const std::uint8_t* pixels,
        bool srgb,
        bool mipmaps
    )
    {
        if (!device || !pixels || width == 0 || height == 0)
        {
            std::cerr << "Cannot create a texture without a device and data.\n";
            return nullptr;
        }

        const std::uint32_t mipLevels =
            mipmaps ? static_cast<std::uint32_t>(std::bit_width(std::max(width, height))) : 1u;

        SDL_GPUTextureCreateInfo createInfo{};
        createInfo.type = SDL_GPU_TEXTURETYPE_2D;
        createInfo.format = srgb
            ? SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB
            : SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
        // Mip generation blits into each level, so it needs COLOR_TARGET.
        createInfo.usage =
            SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
        createInfo.width = width;
        createInfo.height = height;
        createInfo.layer_count_or_depth = 1;
        createInfo.num_levels = mipLevels;
        createInfo.sample_count = SDL_GPU_SAMPLECOUNT_1;

        std::unique_ptr<Texture> texture(new Texture(device));
        texture->m_width = width;
        texture->m_height = height;
        texture->m_texture = SDL_CreateGPUTexture(device, &createInfo);
        if (!texture->m_texture)
        {
            std::cerr
                << "Failed to create GPU texture: "
                << SDL_GetError()
                << '\n';
            return nullptr;
        }

        const Uint32 byteSize = width * height * 4;

        SDL_GPUTransferBufferCreateInfo transferInfo{};
        transferInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
        transferInfo.size = byteSize;

        SDL_GPUTransferBuffer* transfer =
            SDL_CreateGPUTransferBuffer(device, &transferInfo);
        if (!transfer)
        {
            std::cerr
                << "Failed to create texture transfer buffer: "
                << SDL_GetError()
                << '\n';
            return nullptr;
        }

        void* mapped = SDL_MapGPUTransferBuffer(device, transfer, false);
        if (!mapped)
        {
            std::cerr
                << "Failed to map texture transfer buffer: "
                << SDL_GetError()
                << '\n';
            SDL_ReleaseGPUTransferBuffer(device, transfer);
            return nullptr;
        }
        std::memcpy(mapped, pixels, byteSize);
        SDL_UnmapGPUTransferBuffer(device, transfer);

        SDL_GPUCommandBuffer* commandBuffer =
            SDL_AcquireGPUCommandBuffer(device);
        if (!commandBuffer)
        {
            std::cerr
                << "Failed to acquire texture upload command buffer: "
                << SDL_GetError()
                << '\n';
            SDL_ReleaseGPUTransferBuffer(device, transfer);
            return nullptr;
        }

        SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(commandBuffer);

        SDL_GPUTextureTransferInfo source{};
        source.transfer_buffer = transfer;
        source.pixels_per_row = width;
        source.rows_per_layer = height;

        SDL_GPUTextureRegion destination{};
        destination.texture = texture->m_texture;
        destination.w = width;
        destination.h = height;
        destination.d = 1;

        SDL_UploadToGPUTexture(copyPass, &source, &destination, false);
        SDL_EndGPUCopyPass(copyPass);

        if (mipLevels > 1)
        {
            SDL_GenerateMipmapsForGPUTexture(commandBuffer, texture->m_texture);
        }

        const bool submitted = SDL_SubmitGPUCommandBuffer(commandBuffer);
        if (!submitted)
        {
            std::cerr
                << "Failed to submit texture upload: "
                << SDL_GetError()
                << '\n';
        }

        SDL_ReleaseGPUTransferBuffer(device, transfer);

        if (!submitted)
        {
            return nullptr;
        }

        return texture;
    }

    std::unique_ptr<Texture> Texture::CreateRenderTarget(
        SDL_GPUDevice* device,
        std::uint32_t width,
        std::uint32_t height
    )
    {
        if (!device || width == 0 || height == 0)
        {
            return nullptr;
        }
        SDL_GPUTextureCreateInfo createInfo{};
        createInfo.type = SDL_GPU_TEXTURETYPE_2D;
        createInfo.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB;
        createInfo.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
        createInfo.width = width;
        createInfo.height = height;
        createInfo.layer_count_or_depth = 1;
        createInfo.num_levels = 1;
        createInfo.sample_count = SDL_GPU_SAMPLECOUNT_1;

        std::unique_ptr<Texture> texture(new Texture(device));
        texture->m_width = width;
        texture->m_height = height;
        texture->m_pixelArt = true;
        texture->m_texture = SDL_CreateGPUTexture(device, &createInfo);
        if (!texture->m_texture)
        {
            std::cerr << "Failed to create render target: " << SDL_GetError() << '\n';
            return nullptr;
        }
        return texture;
    }

    Texture::Texture(SDL_GPUDevice* device) : m_device(device)
    {
        GpuResources::Added(m_device, GpuResourceKind::Texture);
    }

    Texture::~Texture()
    {
        GpuResources::Removed(m_device, GpuResourceKind::Texture);
        if (m_texture)
        {
            SDL_ReleaseGPUTexture(m_device, m_texture);
        }
    }
}
