#include "Physics/CollisionWorld.h"

#include <cgltf.h>

#include <glm/geometric.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <memory>

namespace Atom
{
    namespace
    {
        // Floors must face up at least this much (cos of ~50 degrees).
        constexpr float MinFloorNormalY = 0.65f;

        // Ericson, Real-Time Collision Detection, 5.1.5.
        glm::vec3 ClosestPointOnTriangle(
            const glm::vec3& p,
            const glm::vec3& a,
            const glm::vec3& b,
            const glm::vec3& c
        )
        {
            const glm::vec3 ab = b - a;
            const glm::vec3 ac = c - a;
            const glm::vec3 ap = p - a;
            const float d1 = glm::dot(ab, ap);
            const float d2 = glm::dot(ac, ap);
            if (d1 <= 0.0f && d2 <= 0.0f) { return a; }

            const glm::vec3 bp = p - b;
            const float d3 = glm::dot(ab, bp);
            const float d4 = glm::dot(ac, bp);
            if (d3 >= 0.0f && d4 <= d3) { return b; }

            const float vc = d1 * d4 - d3 * d2;
            if (vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f)
            {
                return a + ab * (d1 / (d1 - d3));
            }

            const glm::vec3 cp = p - c;
            const float d5 = glm::dot(ab, cp);
            const float d6 = glm::dot(ac, cp);
            if (d6 >= 0.0f && d5 <= d6) { return c; }

            const float vb = d5 * d2 - d1 * d6;
            if (vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f)
            {
                return a + ac * (d2 / (d2 - d6));
            }

            const float va = d3 * d6 - d5 * d4;
            if (va <= 0.0f && (d4 - d3) >= 0.0f && (d5 - d6) >= 0.0f)
            {
                return b + (c - b) * ((d4 - d3) / ((d4 - d3) + (d5 - d6)));
            }

            const float denom = 1.0f / (va + vb + vc);
            return a + ab * (vb * denom) + ac * (vc * denom);
        }

        struct GltfDeleter
        {
            void operator()(cgltf_data* data) const { cgltf_free(data); }
        };
    }

    bool CollisionWorld::Load(const std::string& path)
    {
        m_triangles.clear();

        cgltf_options options{};
        cgltf_data* rawData = nullptr;
        if (cgltf_parse_file(&options, path.c_str(), &rawData)
            != cgltf_result_success)
        {
            std::cerr << "Failed to parse collision glTF '" << path << "'.\n";
            return false;
        }
        std::unique_ptr<cgltf_data, GltfDeleter> data(rawData);

        if (cgltf_load_buffers(&options, data.get(), path.c_str())
            != cgltf_result_success)
        {
            std::cerr << "Failed to load collision buffers '" << path << "'.\n";
            return false;
        }

        for (cgltf_size n = 0; n < data->nodes_count; ++n)
        {
            const cgltf_node& node = data->nodes[n];
            if (!node.mesh)
            {
                continue;
            }

            glm::mat4 world{ 1.0f };
            cgltf_node_transform_world(&node, glm::value_ptr(world));

            for (cgltf_size p = 0; p < node.mesh->primitives_count; ++p)
            {
                const cgltf_primitive& primitive = node.mesh->primitives[p];
                if (primitive.type != cgltf_primitive_type_triangles)
                {
                    continue;
                }

                const cgltf_accessor* positions = nullptr;
                for (cgltf_size a = 0; a < primitive.attributes_count; ++a)
                {
                    if (primitive.attributes[a].type
                        == cgltf_attribute_type_position)
                    {
                        positions = primitive.attributes[a].data;
                    }
                }
                if (!positions)
                {
                    continue;
                }

                const auto vertex = [&](cgltf_size index) {
                    glm::vec3 local{ 0.0f };
                    cgltf_accessor_read_float(
                        positions, index, glm::value_ptr(local), 3);
                    return glm::vec3(world * glm::vec4(local, 1.0f));
                };
                const auto index = [&](cgltf_size i) {
                    return primitive.indices
                        ? cgltf_accessor_read_index(primitive.indices, i)
                        : i;
                };

                const cgltf_size count = primitive.indices
                    ? primitive.indices->count
                    : positions->count;
                for (cgltf_size i = 0; i + 2 < count; i += 3)
                {
                    AddTriangle(
                        vertex(index(i)),
                        vertex(index(i + 1)),
                        vertex(index(i + 2))
                    );
                }
            }
        }

        std::cout
            << "Loaded collision '" << path << "': "
            << m_triangles.size() << " triangles\n";
        return !m_triangles.empty();
    }

    void CollisionWorld::AddTriangle(
        const glm::vec3& a,
        const glm::vec3& b,
        const glm::vec3& c
    )
    {
        const glm::vec3 cross = glm::cross(b - a, c - a);
        const float length = glm::length(cross);
        if (length < 1e-8f)
        {
            return;
        }

        Triangle triangle{};
        triangle.a = a;
        triangle.b = b;
        triangle.c = c;
        triangle.normal = cross / length;
        triangle.boundsMin = glm::min(a, glm::min(b, c));
        triangle.boundsMax = glm::max(a, glm::max(b, c));
        m_triangles.push_back(triangle);
    }

    std::optional<RayHit> CollisionWorld::Raycast(
        const glm::vec3& from,
        const glm::vec3& to
    ) const
    {
        const glm::vec3 segment = to - from;
        const float length = glm::length(segment);
        if (length < 1e-6f)
        {
            return std::nullopt;
        }
        const glm::vec3 direction = segment / length;
        const glm::vec3 segmentMin = glm::min(from, to);
        const glm::vec3 segmentMax = glm::max(from, to);

        std::optional<RayHit> nearest;
        float nearestDistance = length;

        for (const Triangle& triangle : m_triangles)
        {
            // Cheap reject: the segment's box must overlap the triangle's.
            if (segmentMax.x < triangle.boundsMin.x || segmentMin.x > triangle.boundsMax.x
                || segmentMax.y < triangle.boundsMin.y || segmentMin.y > triangle.boundsMax.y
                || segmentMax.z < triangle.boundsMin.z || segmentMin.z > triangle.boundsMax.z)
            {
                continue;
            }

            // Möller–Trumbore: solve from + t*dir = a + u*e1 + v*e2.
            const glm::vec3 e1 = triangle.b - triangle.a;
            const glm::vec3 e2 = triangle.c - triangle.a;
            const glm::vec3 p = glm::cross(direction, e2);
            const float determinant = glm::dot(e1, p);
            if (std::abs(determinant) < 1e-8f)
            {
                continue; // parallel to the triangle
            }
            const float inverse = 1.0f / determinant;
            const glm::vec3 s = from - triangle.a;
            const float u = glm::dot(s, p) * inverse;
            if (u < 0.0f || u > 1.0f)
            {
                continue;
            }
            const glm::vec3 q = glm::cross(s, e1);
            const float v = glm::dot(direction, q) * inverse;
            if (v < 0.0f || u + v > 1.0f)
            {
                continue;
            }
            const float t = glm::dot(e2, q) * inverse;
            if (t < 0.0f || t >= nearestDistance)
            {
                continue;
            }

            nearestDistance = t;
            nearest = RayHit{ t, from + direction * t, triangle.normal };
        }

        return nearest;
    }

    bool CollisionWorld::ResolveSphereHorizontal(
        glm::vec3& center,
        float radius
    ) const
    {
        bool hit = false;

        for (const Triangle& triangle : m_triangles)
        {
            // Floors and ceilings never push sideways.
            if (std::abs(triangle.normal.y) > MinFloorNormalY)
            {
                continue;
            }

            if (center.x + radius < triangle.boundsMin.x
                || center.x - radius > triangle.boundsMax.x
                || center.y + radius < triangle.boundsMin.y
                || center.y - radius > triangle.boundsMax.y
                || center.z + radius < triangle.boundsMin.z
                || center.z - radius > triangle.boundsMax.z)
            {
                continue;
            }

            const glm::vec3 closest = ClosestPointOnTriangle(
                center, triangle.a, triangle.b, triangle.c);
            glm::vec3 offset = center - closest;
            offset.y = 0.0f;
            const float distanceSquared = glm::dot(offset, offset);
            if (distanceSquared >= radius * radius)
            {
                continue;
            }

            const float distance = std::sqrt(distanceSquared);
            glm::vec3 push;
            if (distance > 1e-5f)
            {
                push = offset / distance;
            }
            else
            {
                // Centre is on the plane: push along the face normal.
                push = glm::vec3{ triangle.normal.x, 0.0f, triangle.normal.z };
                const float pushLength = glm::length(push);
                if (pushLength < 1e-5f)
                {
                    continue;
                }
                push /= pushLength;
            }

            center += push * (radius - distance);
            hit = true;
        }

        return hit;
    }

    std::optional<float> CollisionWorld::FindFloor(
        const glm::vec3& origin,
        float maxDistance
    ) const
    {
        std::optional<float> best;
        const float lowest = origin.y - maxDistance;

        for (const Triangle& triangle : m_triangles)
        {
            if (triangle.normal.y < MinFloorNormalY
                || origin.x < triangle.boundsMin.x
                || origin.x > triangle.boundsMax.x
                || origin.z < triangle.boundsMin.z
                || origin.z > triangle.boundsMax.z
                || triangle.boundsMin.y > origin.y
                || triangle.boundsMax.y < lowest)
            {
                continue;
            }

            // Vertical ray vs triangle via barycentrics in the XZ plane.
            const glm::vec2 p{ origin.x, origin.z };
            const glm::vec2 a{ triangle.a.x, triangle.a.z };
            const glm::vec2 b{ triangle.b.x, triangle.b.z };
            const glm::vec2 c{ triangle.c.x, triangle.c.z };
            const glm::vec2 v0 = b - a;
            const glm::vec2 v1 = c - a;
            const glm::vec2 v2 = p - a;
            const float denom = v0.x * v1.y - v1.x * v0.y;
            if (std::abs(denom) < 1e-8f)
            {
                continue;
            }
            const float v = (v2.x * v1.y - v1.x * v2.y) / denom;
            const float w = (v0.x * v2.y - v2.x * v0.y) / denom;
            const float u = 1.0f - v - w;
            if (u < 0.0f || v < 0.0f || w < 0.0f)
            {
                continue;
            }

            const float height =
                triangle.a.y * u + triangle.b.y * v + triangle.c.y * w;
            if (height <= origin.y && height >= lowest
                && (!best || height > *best))
            {
                best = height;
            }
        }

        return best;
    }
}
