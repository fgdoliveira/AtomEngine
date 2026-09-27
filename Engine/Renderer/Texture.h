#pragma once

#include <cstdint>
#include <memory>

struct SDL_GPUDevice;
struct SDL_GPUTexture;

namespace Atom
{
    // Immutable 2D RGBA8 texture with a full mip chain.
    // Must be destroyed before the device that created it.
    class Texture
    {
    public:
        // pixels: width * height RGBA8 texels, top row first.
        static std::unique_ptr<Texture> Create(
            SDL_GPUDevice* device,
            std::uint32_t width,
            std::uint32_t height,
            const std::uint8_t* pixels,
            bool srgb = true
        );

        ~Texture();

        Texture(const Texture&) = delete;
        Texture& operator=(const Texture&) = delete;

        SDL_GPUTexture* GetGPUTexture() const { return m_texture; }
        std::uint32_t GetWidth() const { return m_width; }
        std::uint32_t GetHeight() const { return m_height; }

    private:
        explicit Texture(SDL_GPUDevice* device) : m_device(device) {}

        SDL_GPUDevice* m_device = nullptr;
        SDL_GPUTexture* m_texture = nullptr;
        std::uint32_t m_width = 0;
        std::uint32_t m_height = 0;
    };
}
