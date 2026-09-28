#pragma once

#include <glm/vec3.hpp>

#include <optional>
#include <string>
#include <vector>

namespace Atom
{
    // Static world-space triangle soup for character collision.
    // Loaded from a glTF whose node transforms are baked in.
    struct RayHit
    {
        float distance = 0.0f; // from the segment start, in metres
        glm::vec3 point{ 0.0f };
        glm::vec3 normal{ 0.0f };
    };

    class CollisionWorld
    {
    public:
        bool Load(const std::string& path);

        // Adds one world-space triangle (e.g. procedural geometry, tests).
        // Degenerate triangles are ignored.
        void AddTriangle(const glm::vec3& a, const glm::vec3& b, const glm::vec3& c);

        // Nearest hit along the segment from -> to, from either side of a
        // triangle. Used for line of sight.
        std::optional<RayHit> Raycast(const glm::vec3& from, const glm::vec3& to) const;

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
