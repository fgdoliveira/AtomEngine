#pragma once

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include <array>
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
        // Baked light (linear, 0..1 as 16-bit fixed point); white = unlit
        // by any bake. Read in the shader as a float4.
        std::array<std::uint16_t, 4> color{ 65535, 65535, 65535, 65535 };
    };

    // GPU-resident indexed triangle mesh. Must be destroyed before the
    // device that created it.
    class Mesh
    {
    public:
        static std::unique_ptr<Mesh> Create(
            SDL_GPUDevice* device,
            std::span<const Vertex> vertices,
            std::span<const std::uint32_t> indices,
            bool hasBakedLight = false
        );

        ~Mesh();

        Mesh(const Mesh&) = delete;
        Mesh& operator=(const Mesh&) = delete;

        SDL_GPUBuffer* GetVertexBuffer() const { return m_vertexBuffer; }
        SDL_GPUBuffer* GetIndexBuffer() const { return m_indexBuffer; }
        std::uint32_t GetIndexCount() const { return m_indexCount; }

        // True when the vertex colours carry baked light (glTF COLOR_0);
        // the shader then uses them instead of the flat hemisphere ambient.
        bool HasBakedLight() const { return m_hasBakedLight; }

        // Local-space axis-aligned bounds of the vertices.
        const glm::vec3& GetBoundsMin() const { return m_boundsMin; }
        const glm::vec3& GetBoundsMax() const { return m_boundsMax; }

    private:
        explicit Mesh(SDL_GPUDevice* device) : m_device(device) {}

        SDL_GPUDevice* m_device = nullptr;
        SDL_GPUBuffer* m_vertexBuffer = nullptr;
        SDL_GPUBuffer* m_indexBuffer = nullptr;
        std::uint32_t m_indexCount = 0;
        bool m_hasBakedLight = false;
        glm::vec3 m_boundsMin{ 0.0f };
        glm::vec3 m_boundsMax{ 0.0f };
    };
}
