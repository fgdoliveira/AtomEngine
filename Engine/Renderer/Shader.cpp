#include "Renderer/Shader.h"

#include <SDL3/SDL.h>

#include <iostream>
#include <string>

namespace Atom
{
    SDL_GPUShader* LoadShader(
        SDL_GPUDevice* device,
        const char* name,
        SDL_GPUShaderStage stage,
        const ShaderResources& resources
    )
    {
        const char* basePath = SDL_GetBasePath();
        const std::string path =
            std::string(basePath ? basePath : "") + "shaders/" + name + ".dxil";

        size_t codeSize = 0;
        void* code = SDL_LoadFile(path.c_str(), &codeSize);
        if (!code)
        {
            std::cerr
                << "Failed to load shader '" << path << "': "
                << SDL_GetError()
                << '\n';
            return nullptr;
        }

        SDL_GPUShaderCreateInfo createInfo{};
        createInfo.code_size = codeSize;
        createInfo.code = static_cast<const Uint8*>(code);
        createInfo.entrypoint = "main";
        createInfo.format = SDL_GPU_SHADERFORMAT_DXIL;
        createInfo.stage = stage;
        createInfo.num_samplers = resources.samplers;
        createInfo.num_storage_textures = resources.storageTextures;
        createInfo.num_storage_buffers = resources.storageBuffers;
        createInfo.num_uniform_buffers = resources.uniformBuffers;

        SDL_GPUShader* shader = SDL_CreateGPUShader(device, &createInfo);
        SDL_free(code);

        if (!shader)
        {
            std::cerr
                << "Failed to create shader '" << name << "': "
                << SDL_GetError()
                << '\n';
        }

        return shader;
    }
}
