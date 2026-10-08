#include "Flashlight.h"

#include "Level/JsonText.h"
#include "Physics/CollisionWorld.h"

#include <glm/geometric.hpp>
#include <glm/trigonometric.hpp>

#include <cmath>
#include <cstdio>

namespace AtomGame
{
    namespace
    {
        // A point counts as lit when the beam there is at least this bright
        // (cone x falloff x intensity): the edge of a usable light, not the
        // last faint glow.
        constexpr float LitThreshold = 0.08f;
        // Where the hand holds it, from the eye.
        constexpr float HandRight = 0.18f;
        constexpr float HandDown = 0.15f;
        constexpr float HandForward = 0.1f;
    }

    Flashlight::Flashlight()
    {
        // An old incandescent torch: warm, a tight hot centre and a wide
        // soft spill.
        m_light.range = 14.0f;
        m_light.innerAngleDegrees = 10.0f;
        m_light.outerAngleDegrees = 24.0f;
        m_light.color = glm::vec3{ 1.0f, 0.9f, 0.74f };
        m_light.intensity = 22.0f;
        m_light.specular = 1.0f;
    }

    void Flashlight::SetOwned(bool owned)
    {
        if (owned && !m_owned)
        {
            m_on = true; // found: it comes on in your hand
        }
        m_owned = owned;
        if (!owned)
        {
            m_on = false;
        }
    }

    bool Flashlight::Toggle()
    {
        if (!m_owned)
        {
            return false;
        }
        m_on = !m_on;
        return true;
    }

    void Flashlight::Update(const glm::vec3& eye, const glm::vec3& forward, const glm::vec3& right,
                            float deltaSeconds, bool snap)
    {
        m_time += deltaSeconds;
        // Lag: the aim eases toward the view (frame-rate independent).
        const float follow = snap ? 1.0f : 1.0f - std::exp(-followRate * deltaSeconds);
        m_aim = glm::normalize(m_aim + (forward - m_aim) * follow);

        // Sway: two slow sines at unrelated rates, so it never repeats
        // visibly - a hand, not a machine.
        const glm::vec3 up = glm::normalize(glm::cross(right, forward));
        const float sway = glm::radians(swayDegrees);
        const glm::vec3 wobble = right * (std::sin(m_time * 1.3f) * sway)
            + up * (std::sin(m_time * 0.9f + 1.7f) * sway * 0.7f);

        m_light.position = eye + right * HandRight - up * HandDown + forward * HandForward;
        m_light.direction = glm::normalize(m_aim + wobble);
    }

    std::string Flashlight::LoadSettings(std::string_view json)
    {
        nlohmann::json root;
        if (std::string error = ParseJsonText(json, root); !error.empty())
        {
            return error;
        }
        if (!root.is_object())
        {
            return "the flashlight's settings must be an object";
        }
        // Read into a copy: a bad value anywhere leaves the light as it was.
        Atom::SpotLight light = m_light;
        const auto number = [&](const char* key, float& value, float low, float high) -> std::string {
            const auto found = root.find(key);
            if (found == root.end())
            {
                return {};
            }
            if (!found->is_number() || found->get<float>() < low || found->get<float>() > high)
            {
                return std::string("\"") + key + "\" must be a number from " + std::to_string(low) + " to "
                    + std::to_string(high);
            }
            value = found->get<float>();
            return {};
        };
        for (const std::string& error : {
                 number("range", light.range, 0.5f, 100.0f),
                 number("inner", light.innerAngleDegrees, 0.5f, 80.0f),
                 number("outer", light.outerAngleDegrees, 0.5f, 85.0f),
                 number("intensity", light.intensity, 0.0f, 500.0f),
                 number("specular", light.specular, 0.0f, 10.0f),
                 number("shadowNormalOffset", light.shadowNormalOffset, 0.0f, 0.1f) })
        {
            if (!error.empty())
            {
                return error;
            }
        }
        if (light.outerAngleDegrees < light.innerAngleDegrees)
        {
            return "\"outer\" must be at least \"inner\"";
        }
        if (const auto color = root.find("color"); color != root.end())
        {
            if (!color->is_array() || color->size() != 3 || !(*color)[0].is_number()
                || !(*color)[1].is_number() || !(*color)[2].is_number())
            {
                return "\"color\" must be [r, g, b]";
            }
            light.color = glm::vec3{ (*color)[0].get<float>(), (*color)[1].get<float>(), (*color)[2].get<float>() };
        }
        if (const auto shadows = root.find("shadows"); shadows != root.end())
        {
            if (!shadows->is_boolean())
            {
                return "\"shadows\" must be true or false";
            }
            light.castsShadows = shadows->get<bool>();
        }
        light.beam = m_light.beam; // the level's
        m_light = light;
        return {};
    }

    std::string Flashlight::SaveSettings() const
    {
        char json[512];
        std::snprintf(json, sizeof(json),
            "{\n"
            "  \"$schema\": \"../Schemas/flashlight.schema.json\",\n"
            "  \"range\": %.3g,\n"
            "  \"inner\": %.3g,\n"
            "  \"outer\": %.3g,\n"
            "  \"intensity\": %.3g,\n"
            "  \"color\": [%.3g, %.3g, %.3g],\n"
            "  \"specular\": %.3g,\n"
            "  \"shadows\": %s,\n"
            "  \"shadowNormalOffset\": %.3g\n"
            "}\n",
            m_light.range, m_light.innerAngleDegrees, m_light.outerAngleDegrees, m_light.intensity,
            m_light.color.r, m_light.color.g, m_light.color.b, m_light.specular,
            m_light.castsShadows ? "true" : "false", m_light.shadowNormalOffset);
        return json;
    }

    bool Flashlight::Lights(const glm::vec3& point, const Atom::CollisionWorld* world) const
    {
        if (!IsOn())
        {
            return false;
        }
        const glm::vec3 toPoint = point - m_light.position;
        const float distance = glm::length(toPoint);
        if (distance >= m_light.range || distance < 1e-3f)
        {
            return false;
        }
        const float strength = Atom::SpotMath::Cone(m_light, toPoint)
            * Atom::SpotMath::Falloff(distance, m_light.range) * m_light.intensity;
        if (strength < LitThreshold)
        {
            return false;
        }
        // The same question the shadow map answers, asked of the collision
        // world: does anything stand between the lamp and the point? Hits
        // just before the point are the surface it lies on.
        if (world)
        {
            if (const auto hit = world->Raycast(m_light.position, point); hit && hit->distance < distance - 0.25f)
            {
                return false;
            }
        }
        return true;
    }
}
