#pragma once

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include <cstdint>

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

    // Queries use a uniform grid over the XZ plane (broad phase): each one
    // visits only the triangles in the cells its bounds overlap, instead of
    // every triangle in the level. The grid is rebuilt lazily after
    // triangles change. Not thread-safe (queries share scratch marks).
    class CollisionWorld
    {
    public:
        // Replaces the contents with the triangles of a glTF file.
        bool Load(const std::string& path);
        // Adds a file's triangles to what's there (level chunks, M22).
        bool Append(const std::string& path);
        void Clear();

        // Tests compare the grid against a plain scan of every triangle.
        void SetBroadPhase(bool enabled) { m_useGrid = enabled; }

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

        static constexpr float CellSize = 4.0f; // metres

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

        // Calls f(triangle) once for each triangle whose cells overlap the
        // XZ box, or for every triangle when the grid is off.
        template <typename F>
        void ForEachCandidate(glm::vec2 min, glm::vec2 max, F&& f) const;
        void BuildGrid() const;

        std::vector<Triangle> m_triangles;

        bool m_useGrid = true;
        mutable bool m_gridDirty = true;
        mutable glm::ivec2 m_gridOrigin{ 0 }; // cell coordinates of cell [0]
        mutable glm::ivec2 m_gridSize{ 0 };
        mutable std::vector<std::vector<std::uint32_t>> m_cells;
        mutable std::vector<std::uint32_t> m_marks; // per triangle: last query that saw it
        mutable std::uint32_t m_query = 0;
        mutable std::vector<std::uint32_t> m_candidates;
    };
}
