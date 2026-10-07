#pragma once

#include <cstdint>
#include <memory>

struct SDL_GPUDevice;
struct SDL_GPUTexture;

namespace Atom
{
    // Immutable 2D RGBA8 texture with a full mip chain.
    // Must be destroyed before the device that created it - checked at
    // renderer shutdown (GpuResources).
    class Texture
    {
    public:
        // pixels: width * height RGBA8 texels, top row first. `mipmaps`
        // false gives one level: for atlases drawn at their own size (font
        // glyphs), where a smaller level only blurs and mixes neighbours.
        static std::unique_ptr<Texture> Create(
            SDL_GPUDevice* device,
            std::uint32_t width,
            std::uint32_t height,
            const std::uint8_t* pixels,
            bool srgb = true,
            bool mipmaps = true
        );

        // A colour target the GPU draws into (M27): one mip level, RGBA8
        // sRGB, sampled with nearest filtering (IsPixelArt).
        static std::unique_ptr<Texture> CreateRenderTarget(
            SDL_GPUDevice* device,
            std::uint32_t width,
            std::uint32_t height
        );

        ~Texture();

        Texture(const Texture&) = delete;
        Texture& operator=(const Texture&) = delete;

        SDL_GPUTexture* GetGPUTexture() const { return m_texture; }
        std::uint32_t GetWidth() const { return m_width; }
        std::uint32_t GetHeight() const { return m_height; }
        // Sampled with nearest filtering: big crisp texels up close.
        bool IsPixelArt() const { return m_pixelArt; }

    private:
        explicit Texture(SDL_GPUDevice* device);

        SDL_GPUDevice* m_device = nullptr;
        SDL_GPUTexture* m_texture = nullptr;
        std::uint32_t m_width = 0;
        std::uint32_t m_height = 0;
        bool m_pixelArt = false;
    };
}
