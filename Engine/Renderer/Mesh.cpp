#include "Renderer/Mesh.h"

#include <SDL3/SDL.h>

#include <glm/common.hpp>

#include <cstddef>
#include <cstring>
#include <iostream>

namespace Atom
{
    namespace
    {
        SDL_GPUBuffer* CreateBuffer(
            SDL_GPUDevice* device,
            SDL_GPUBufferUsageFlags usage,
            Uint32 size
        )
        {
            SDL_GPUBufferCreateInfo createInfo{};
            createInfo.usage = usage;
            createInfo.size = size;

            SDL_GPUBuffer* buffer = SDL_CreateGPUBuffer(device, &createInfo);
            if (!buffer)
            {
                std::cerr
                    << "Failed to create GPU buffer: "
                    << SDL_GetError()
                    << '\n';
            }
            return buffer;
        }
    }

    std::unique_ptr<Mesh> Mesh::Create(
        SDL_GPUDevice* device,
        std::span<const Vertex> vertices,
        std::span<const std::uint32_t> indices,
        bool hasBakedLight
    )
    {
        if (!device || vertices.empty() || indices.empty())
        {
            std::cerr << "Cannot create a mesh without a device and data.\n";
            return nullptr;
        }

        const Uint32 vertexBytes =
            static_cast<Uint32>(vertices.size_bytes());
        const Uint32 indexBytes =
            static_cast<Uint32>(indices.size_bytes());

        std::unique_ptr<Mesh> mesh(new Mesh(device));
        mesh->m_indexCount = static_cast<std::uint32_t>(indices.size());
        mesh->m_hasBakedLight = hasBakedLight;
        mesh->m_boundsMin = vertices.front().position;
        mesh->m_boundsMax = vertices.front().position;
        for (const Vertex& vertex : vertices)
        {
            mesh->m_boundsMin = glm::min(mesh->m_boundsMin, vertex.position);
            mesh->m_boundsMax = glm::max(mesh->m_boundsMax, vertex.position);
        }
        mesh->m_vertexBuffer =
            CreateBuffer(device, SDL_GPU_BUFFERUSAGE_VERTEX, vertexBytes);
        mesh->m_indexBuffer =
            CreateBuffer(device, SDL_GPU_BUFFERUSAGE_INDEX, indexBytes);
        if (!mesh->m_vertexBuffer || !mesh->m_indexBuffer)
        {
            return nullptr;
        }

        SDL_GPUTransferBufferCreateInfo transferInfo{};
        transferInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
        transferInfo.size = vertexBytes + indexBytes;

        SDL_GPUTransferBuffer* transfer =
            SDL_CreateGPUTransferBuffer(device, &transferInfo);
        if (!transfer)
        {
            std::cerr
                << "Failed to create GPU transfer buffer: "
                << SDL_GetError()
                << '\n';
            return nullptr;
        }

        auto* mapped = static_cast<std::byte*>(
            SDL_MapGPUTransferBuffer(device, transfer, false));
        if (!mapped)
        {
            std::cerr
                << "Failed to map GPU transfer buffer: "
                << SDL_GetError()
                << '\n';
            SDL_ReleaseGPUTransferBuffer(device, transfer);
            return nullptr;
        }

        std::memcpy(mapped, vertices.data(), vertexBytes);
        std::memcpy(mapped + vertexBytes, indices.data(), indexBytes);
        SDL_UnmapGPUTransferBuffer(device, transfer);

        SDL_GPUCommandBuffer* commandBuffer =
            SDL_AcquireGPUCommandBuffer(device);
        if (!commandBuffer)
        {
            std::cerr
                << "Failed to acquire upload command buffer: "
                << SDL_GetError()
                << '\n';
            SDL_ReleaseGPUTransferBuffer(device, transfer);
            return nullptr;
        }

        SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(commandBuffer);

        SDL_GPUTransferBufferLocation source{};
        source.transfer_buffer = transfer;

        SDL_GPUBufferRegion destination{};
        destination.buffer = mesh->m_vertexBuffer;
        destination.size = vertexBytes;
        SDL_UploadToGPUBuffer(copyPass, &source, &destination, false);

        source.offset = vertexBytes;
        destination.buffer = mesh->m_indexBuffer;
        destination.size = indexBytes;
        SDL_UploadToGPUBuffer(copyPass, &source, &destination, false);

        SDL_EndGPUCopyPass(copyPass);

        const bool submitted = SDL_SubmitGPUCommandBuffer(commandBuffer);
        if (!submitted)
        {
            std::cerr
                << "Failed to submit mesh upload: "
                << SDL_GetError()
                << '\n';
        }

        // SDL defers the actual release until the GPU is done with the copy.
        SDL_ReleaseGPUTransferBuffer(device, transfer);

        if (!submitted)
        {
            return nullptr;
        }

        return mesh;
    }

    Mesh::~Mesh()
    {
        if (m_vertexBuffer)
        {
            SDL_ReleaseGPUBuffer(m_device, m_vertexBuffer);
        }
        if (m_indexBuffer)
        {
            SDL_ReleaseGPUBuffer(m_device, m_indexBuffer);
        }
    }
}
