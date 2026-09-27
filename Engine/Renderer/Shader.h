#pragma once

#include <SDL3/SDL_gpu.h>

#include <cstdint>

namespace Atom
{
    struct ShaderResources
    {
        std::uint32_t samplers = 0;
        std::uint32_t storageTextures = 0;
        std::uint32_t storageBuffers = 0;
        std::uint32_t uniformBuffers = 0;
    };

    // Loads shaders/<name>.dxil from the executable directory.
    // Returns nullptr on failure; the caller owns the shader.
    SDL_GPUShader* LoadShader(
        SDL_GPUDevice* device,
        const char* name,
        SDL_GPUShaderStage stage,
        const ShaderResources& resources
    );
}
