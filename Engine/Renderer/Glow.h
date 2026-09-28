#pragma once

#include <SDL3/SDL_gpu.h>

#include <cstdint>

namespace Atom
{
    // Period glow (M23): the bright parts of the scene, blurred and added
    // back in the post pass, so signs, lamps and lit windows bleed light
    // into the night air.
    //
    //   scene --bright pass, 1/4 size--> A --blur x--> B --blur y--> A
    //
    // Working at a quarter of the scene's size keeps it cheap and makes the
    // blur wide: a 9-tap blur there spans ~36 scene pixels. Two passes of a
    // separable Gaussian (one horizontal, one vertical) do what a 2D kernel
    // would at a fraction of the taps.
    class Glow
    {
    public:
        bool Initialize(SDL_GPUDevice* device, SDL_GPUTextureFormat format);
        void Release();

        // Renders the glow of `scene` (width x height). Pixels brighter than
        // `threshold` contribute, with a soft knee. Returns false on failure.
        bool Render(SDL_GPUCommandBuffer* commandBuffer, SDL_GPUTexture* scene,
                    std::uint32_t width, std::uint32_t height, float threshold);

        // The blurred result for the post pass (valid after Render).
        SDL_GPUTexture* GetTexture() const { return m_textures[0]; }

    private:
        bool EnsureTextures(std::uint32_t width, std::uint32_t height);
        bool Pass(SDL_GPUCommandBuffer* commandBuffer, SDL_GPUGraphicsPipeline* pipeline,
                  SDL_GPUTexture* source, SDL_GPUTexture* target, const void* uniforms,
                  std::uint32_t uniformBytes);

        SDL_GPUDevice* m_device = nullptr;
        SDL_GPUTextureFormat m_format = SDL_GPU_TEXTUREFORMAT_INVALID;
        SDL_GPUGraphicsPipeline* m_brightPipeline = nullptr;
        SDL_GPUGraphicsPipeline* m_blurPipeline = nullptr;
        SDL_GPUSampler* m_sampler = nullptr;
        SDL_GPUTexture* m_textures[2]{};
        std::uint32_t m_width = 0;
        std::uint32_t m_height = 0;
    };
}
