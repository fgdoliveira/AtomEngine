#pragma once

#include <glm/vec3.hpp>

#include <optional>
#include <string>
#include <vector>

namespace Atom
{
    // Static world-space triangle soup for character collision.
    // Loaded from a glTF whose node transforms are baked in.
    class CollisionWorld
    {
    public:
        bool Load(const std::string& path);

        // Pushes a sphere out of every triangle it overlaps, horizontally
        // only (walls). Returns true if anything was hit.
        bool ResolveSphereHorizontal(glm::vec3& center, float radius) const;

        // Highest walkable surface under `origin`, searching down at most
        // `maxDistance`. Only triangles facing mostly upward count.
        std::optional<float> FindFloor(
            const glm::vec3& origin,
            float maxDistance
        ) const;

        std::size_t GetTriangleCount() const { return m_triangles.size(); }

    private:
        struct Triangle
        {
            glm::vec3 a;
            glm::vec3 b;
            glm::vec3 c;
            glm::vec3 normal;
            glm::vec3 boundsMin;
            glm::vec3 boundsMax;
        };

        std::vector<Triangle> m_triangles;
    };
}
