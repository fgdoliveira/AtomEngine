#pragma once

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include <cstdint>
#include <memory>
#include <span>

struct SDL_GPUBuffer;
struct SDL_GPUDevice;

namespace Atom
{
    struct Vertex
    {
        glm::vec3 position;
        glm::vec3 normal;
        glm::vec2 uv;
    };

    // GPU-resident indexed triangle mesh. Must be destroyed before the
    // device that created it.
    class Mesh
    {
    public:
        static std::unique_ptr<Mesh> Create(
            SDL_GPUDevice* device,
            std::span<const Vertex> vertices,
            std::span<const std::uint32_t> indices
        );

        ~Mesh();

        Mesh(const Mesh&) = delete;
        Mesh& operator=(const Mesh&) = delete;

        SDL_GPUBuffer* GetVertexBuffer() const { return m_vertexBuffer; }
        SDL_GPUBuffer* GetIndexBuffer() const { return m_indexBuffer; }
        std::uint32_t GetIndexCount() const { return m_indexCount; }

    private:
        explicit Mesh(SDL_GPUDevice* device) : m_device(device) {}

        SDL_GPUDevice* m_device = nullptr;
        SDL_GPUBuffer* m_vertexBuffer = nullptr;
        SDL_GPUBuffer* m_indexBuffer = nullptr;
        std::uint32_t m_indexCount = 0;
    };
}
