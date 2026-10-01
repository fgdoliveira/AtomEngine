#pragma once

#include <glm/common.hpp>
#include <glm/exponential.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/mat4x4.hpp>
#include <glm/trigonometric.hpp>
#include <glm/vec3.hpp>

#include <algorithm>

namespace Atom
{
    // A spot light (M42): a point light that shines into a cone, like a
    // flashlight or a stage lamp. Full strength inside the inner angle,
    // fading to nothing at the outer angle; the falloff with distance ends
    // exactly at `range`, so the light touches nothing beyond it.
    struct SpotLight
    {
        glm::vec3 position{ 0.0f };
        glm::vec3 direction{ 0.0f, 0.0f, -1.0f }; // where it points (normalised by the renderer)
        float range = 15.0f;                       // metres: no light beyond
        float innerAngleDegrees = 12.0f;           // half-angle of full strength
        float outerAngleDegrees = 24.0f;           // half-angle where it ends
        glm::vec3 color{ 1.0f, 0.95f, 0.85f };     // linear
        float intensity = 20.0f;                   // at 1 m, on axis (a torch reaches ~8 m)
        float specular = 1.0f;                     // scales every highlight it makes

        // Its own shadow map (M43): a depth image seen from the lamp.
        bool castsShadows = true;
        // Pushes each lookup off its surface along the normal, against
        // shadow acne. Per metre from the lamp: a shadow-map texel covers
        // more of the world the further away it is (perspective).
        float shadowNormalOffset = 0.004f;

        // A visible beam in the air (M45): 0 = none; ~0.1 a dusty cellar.
        float beam = 0.0f;
    };

    // The same maths as Shaders/Basic.frag.hlsl (SpotLighting), on the CPU,
    // so it can be tested and reasoned about without a GPU. Keep the two in
    // step.
    namespace SpotMath
    {
        // 1 inside the inner cone, 0 outside the outer one, a smooth ramp
        // between (smoothstep on the cosines: cheaper than angles, and the
        // ramp still reads as a soft edge).
        inline float Cone(const SpotLight& light, const glm::vec3& toPoint)
        {
            const float cosInner = std::cos(glm::radians(light.innerAngleDegrees));
            const float cosOuter = std::cos(glm::radians(light.outerAngleDegrees));
            const float along = glm::dot(glm::normalize(toPoint), glm::normalize(light.direction));
            return glm::smoothstep(cosOuter, cosInner, along);
        }

        // Inverse-square (real light spreads over the sphere's area), with
        // +1 so it doesn't blow up at the lamp, times a window that brings
        // it smoothly to exactly 0 at the range.
        inline float Falloff(float distance, float range)
        {
            const float ratio = distance / range;
            const float window = std::clamp(1.0f - ratio * ratio * ratio * ratio, 0.0f, 1.0f);
            return window * window / (distance * distance + 1.0f);
        }

        // Blinn-Phong exponent from glTF roughness (0 smooth .. 1 rough):
        // the usual mapping through alpha = roughness^2, clamped so rough
        // surfaces still get a broad sheen and smooth ones a small spot.
        inline float Shininess(float roughness)
        {
            const float alpha = std::max(roughness * roughness, 1e-3f);
            return std::clamp(2.0f / (alpha * alpha) - 2.0f, 2.0f, 256.0f);
        }

        // How strong a material's highlight is: its own "atom_specular" if
        // given (>= 0), otherwise more for smoother surfaces.
        inline float SpecularStrength(float roughness, float authored)
        {
            return authored >= 0.0f ? authored : 0.5f * (1.0f - std::clamp(roughness, 0.0f, 1.0f));
        }

        // The spot's view and projection for its shadow map: from the lamp,
        // along its axis, a square frustum wide enough for the outer cone
        // (plus a little, so PCF taps at the edge stay inside) and as deep
        // as its range. Points it lights land in clip space x,y in [-1, 1]
        // and depth in [0, 1].
        inline glm::mat4 ViewProjection(const SpotLight& light)
        {
            const glm::vec3 forward = glm::normalize(light.direction);
            // Any up that isn't parallel to where it points.
            const glm::vec3 up = std::abs(forward.y) > 0.99f ? glm::vec3{ 0.0f, 0.0f, 1.0f }
                                                             : glm::vec3{ 0.0f, 1.0f, 0.0f };
            const glm::mat4 view = glm::lookAt(light.position, light.position + forward, up);
            const float fov = glm::radians(std::min(2.0f * light.outerAngleDegrees + 4.0f, 170.0f));
            const glm::mat4 projection = glm::perspective(fov, 1.0f, 0.05f, std::max(light.range, 0.1f));
            return projection * view;
        }

        // Light culling (M46): does a light's sphere of reach touch a box
        // (centre, half-extents)? The distance from the sphere's centre to
        // the nearest point of the box, against the radius.
        inline bool SphereTouchesBox(const glm::vec3& sphere, float radius, const glm::vec3& center,
                                     const glm::vec3& extent)
        {
            const glm::vec3 nearest = glm::clamp(sphere, center - extent, center + extent);
            const glm::vec3 gap = sphere - nearest;
            return glm::dot(gap, gap) <= radius * radius;
        }

        struct Response
        {
            float diffuse = 0.0f;  // times base colour and light colour
            float specular = 0.0f; // times light colour only (highlights are white-ish)
        };

        // What the spot does at `point` (normal `normal`, seen from `eye`).
        inline Response Evaluate(const SpotLight& light, const glm::vec3& point, const glm::vec3& normal,
                                 const glm::vec3& eye, float shininess, float specularStrength)
        {
            const glm::vec3 toLight = light.position - point;
            const float distance = glm::length(toLight);
            const glm::vec3 l = toLight / std::max(distance, 1e-4f);
            const float reach = Cone(light, -toLight) * Falloff(distance, light.range) * light.intensity;
            const float lambert = std::max(glm::dot(normal, l), 0.0f);

            // Blinn-Phong: the half vector between light and eye; the closer
            // the normal is to it, the closer we are to the mirror angle.
            // (n+8)/8 keeps tight highlights from dimming as they narrow.
            // Gated by lambert: no highlight on faces turned away.
            const glm::vec3 h = glm::normalize(l + glm::normalize(eye - point));
            const float spec = std::pow(std::max(glm::dot(normal, h), 0.0f), shininess)
                * (shininess + 8.0f) / 8.0f * specularStrength * light.specular;

            return Response{ reach * lambert, reach * spec * (lambert > 0.0f ? 1.0f : 0.0f) };
        }
    }
}
