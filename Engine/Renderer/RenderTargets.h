#pragma once

#include <SDL3/SDL_gpu.h>

#include <cstdint>

namespace Atom
{
    // Off-screen HDR scene colour + depth, optionally multisampled.
    // Recreated on demand when the size or sample count changes.
    class RenderTargets
    {
    public:
        // Picks the colour format and the sample counts the GPU supports.
        bool Initialize(SDL_GPUDevice* device);
        void Release();

        // Returns false if (re)creation failed.
        bool Ensure(std::uint32_t width, std::uint32_t height, std::uint32_t samples);

        // Highest supported count not above `requested` (1, 2 or 4).
        std::uint32_t ClampSampleCount(std::uint32_t requested) const;

        SDL_GPUTextureFormat GetColorFormat() const { return m_colorFormat; }
        static SDL_GPUTextureFormat GetDepthFormat();

        // Render-pass attachments for the scene pass.
        SDL_GPUColorTargetInfo MakeColorTargetInfo(const SDL_FColor& clear) const;
        SDL_GPUDepthStencilTargetInfo MakeDepthTargetInfo() const;

        // Single-sample result, sampled by the post pass.
        SDL_GPUTexture* GetSceneTexture() const { return m_resolved; }

        std::uint32_t GetWidth() const { return m_width; }
        std::uint32_t GetHeight() const { return m_height; }
        std::uint32_t GetSamples() const { return m_samples; }

    private:
        void ReleaseTextures();

        SDL_GPUDevice* m_device = nullptr;
        SDL_GPUTextureFormat m_colorFormat = SDL_GPU_TEXTUREFORMAT_INVALID;
        std::uint32_t m_maxSamples = 1;

        SDL_GPUTexture* m_multisampled = nullptr; // only when samples > 1
        SDL_GPUTexture* m_resolved = nullptr;
        SDL_GPUTexture* m_depth = nullptr;

        std::uint32_t m_width = 0;
        std::uint32_t m_height = 0;
        std::uint32_t m_samples = 0;
    };
}
