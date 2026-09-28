#include "UneaseDirector.h"

#include "AudioScape.h"
#include "Level/Level.h"
#include "Renderer/Renderer.h"
#include "Scene/Camera.h"

#include <glm/geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <iterator>
#include <vector>

namespace AtomGame
{
    namespace
    {
        constexpr float VanishDistance = 14.0f;   // too close: it's gone
        constexpr float MinAppearDistance = 28.0f;
        constexpr float StaticFullDistance = 20.0f;
        constexpr float StaticSilentDistance = 55.0f;

        float Horizontal(const glm::vec3& a, const glm::vec3& b)
        {
            const glm::vec2 d{ a.x - b.x, a.z - b.z };
            return std::sqrt(d.x * d.x + d.y * d.y);
        }

        // A thin standing silhouette from a few boxes; it only ever reads
        // as a dark shape in the fog, so detail would be wasted.
        std::unique_ptr<Atom::Mesh> BuildFigure(Atom::Renderer& renderer)
        {
            struct Box
            {
                glm::vec3 center;
                glm::vec3 half;
            };
            const Box boxes[] = {
                { { -0.09f, 0.42f, 0.0f }, { 0.07f, 0.42f, 0.08f } },  // legs
                { { 0.09f, 0.42f, 0.0f }, { 0.07f, 0.42f, 0.08f } },
                { { 0.0f, 1.13f, 0.0f }, { 0.19f, 0.30f, 0.11f } },    // torso
                { { -0.25f, 1.02f, 0.0f }, { 0.05f, 0.38f, 0.06f } },  // arms, hanging
                { { 0.25f, 1.02f, 0.0f }, { 0.05f, 0.38f, 0.06f } },
                { { 0.0f, 1.52f, 0.0f }, { 0.05f, 0.06f, 0.05f } },    // neck
                { { 0.0f, 1.66f, 0.01f }, { 0.10f, 0.12f, 0.11f } },   // head
            };

            struct Face
            {
                glm::vec3 normal;
                glm::vec3 up;
            };
            constexpr Face faces[] = {
                { { 1, 0, 0 }, { 0, 1, 0 } }, { { -1, 0, 0 }, { 0, 1, 0 } },
                { { 0, 0, 1 }, { 0, 1, 0 } }, { { 0, 0, -1 }, { 0, 1, 0 } },
                { { 0, 1, 0 }, { 0, 0, -1 } }, { { 0, -1, 0 }, { 0, 0, 1 } },
            };

            std::vector<Atom::Vertex> vertices;
            std::vector<std::uint32_t> indices;
            for (const Box& box : boxes)
            {
                for (const Face& face : faces)
                {
                    const glm::vec3 right = glm::cross(face.up, face.normal);
                    const auto base = static_cast<std::uint32_t>(vertices.size());
                    for (const glm::vec2 corner : { glm::vec2{ -1, -1 }, glm::vec2{ 1, -1 },
                                                     glm::vec2{ 1, 1 }, glm::vec2{ -1, 1 } })
                    {
                        const glm::vec3 local =
                            (face.normal + right * corner.x + face.up * corner.y) * box.half;
                        vertices.push_back({ box.center + local, face.normal, glm::vec2{ 0.0f } });
                    }
                    indices.insert(indices.end(),
                        { base, base + 1, base + 2, base, base + 2, base + 3 });
                }
            }
            return renderer.CreateMesh(vertices, indices);
        }
    }

    bool UneaseDirector::Initialize(Atom::Renderer& renderer)
    {
        m_figureMesh = BuildFigure(renderer);
        m_figureMaterial.baseColorFactor = glm::vec4{ 0.012f, 0.012f, 0.014f, 1.0f };
        return m_figureMesh != nullptr;
    }

    void UneaseDirector::Configure(const LevelUnease& config, Level* level)
    {
        // Called on every level change. Never touch the previous level's
        // material here: it may already be gone.
        m_config = config;
        m_level = level;
        m_flickerScreen = level && !config.flickerMaterial.empty()
            ? level->FindSceneMaterial(config.flickerMaterial)
            : nullptr;
        if (m_flickerScreen)
        {
            m_flickerEmission = m_flickerScreen->emissiveFactor;
        }

        m_figureVisible = false;
        m_figureCooldown = 8.0f;
        m_flickerTime = -1.0f;
        m_flickerCooldown = 20.0f;
    }

    void UneaseDirector::Shutdown()
    {
        m_figureMesh.reset();
    }

    void UneaseDirector::SetEnabled(bool enabled)
    {
        m_enabled = enabled;
        if (!enabled)
        {
            m_figureVisible = false;
            m_flickerTime = -1.0f;
            if (m_flickerScreen)
            {
                m_flickerScreen->emissiveFactor = m_flickerEmission;
            }
        }
    }

    bool UneaseDirector::PlaceFigure(const Atom::Camera& camera, const glm::vec3& playerFeet)
    {
        // Appear only where the player isn't looking, far enough away that
        // the fog keeps it ambiguous.
        const glm::vec3 forward = camera.GetFlatForward();
        std::vector<glm::vec3> candidates;
        if (!m_config.figure)
        {
            return false;
        }
        for (const glm::vec3& spot : m_config.figureSpots)
        {
            const float distance = Horizontal(spot, playerFeet);
            if (distance < MinAppearDistance)
            {
                continue;
            }
            const glm::vec3 toSpot = glm::normalize(glm::vec3{ spot.x - playerFeet.x, 0.0f, spot.z - playerFeet.z });
            if (glm::dot(toSpot, forward) > 0.35f)
            {
                continue; // in view
            }
            candidates.push_back(spot);
        }

        if (candidates.empty())
        {
            return false;
        }

        std::uniform_int_distribution<std::size_t> pick(0, candidates.size() - 1);
        m_figurePosition = candidates[pick(m_random)];
        m_figureVisible = true;
        std::cout
            << "Unease: figure at (" << m_figurePosition.x << ", "
            << m_figurePosition.z << ")\n";
        return true;
    }

    void UneaseDirector::UpdateFigure(
        float deltaSeconds,
        const Atom::Camera& camera,
        const glm::vec3& playerFeet
    )
    {
        if (m_figureVisible)
        {
            // Always turned toward the player.
            const glm::vec3 toPlayer = playerFeet - m_figurePosition;
            m_figureYaw = std::atan2(toPlayer.x, toPlayer.z);

            if (Horizontal(m_figurePosition, playerFeet) < VanishDistance)
            {
                m_figureVisible = false;
                std::uniform_real_distribution<float> wait(25.0f, 45.0f);
                m_figureCooldown = wait(m_random);
            }
        }
        else
        {
            m_figureCooldown -= deltaSeconds;
            if (m_figureCooldown <= 0.0f && !PlaceFigure(camera, playerFeet))
            {
                m_figureCooldown = 3.0f; // nowhere suitable yet; retry soon
            }
        }

        // Static follows proximity to the figure, eased so it breathes in
        // and out rather than switching.
        float target = 0.0f;
        if (m_figureVisible)
        {
            const float distance = Horizontal(m_figurePosition, playerFeet);
            target = std::clamp(
                (StaticSilentDistance - distance) / (StaticSilentDistance - StaticFullDistance),
                0.0f,
                1.0f
            );
        }
        const float rate = target > m_staticLevel ? 0.6f : 2.5f; // fades out faster
        m_staticLevel += (target - m_staticLevel) * std::min(1.0f, rate * deltaSeconds);
    }

    void UneaseDirector::UpdateFlicker(
        float deltaSeconds,
        const glm::vec3& playerFeet,
        AudioScape& /*audio*/
    )
    {
        if (!m_flickerScreen)
        {
            return;
        }

        if (m_flickerTime < 0.0f)
        {
            m_flickerCooldown -= deltaSeconds;
            bool nearMachine = false;
            for (const glm::vec3& machine : m_config.flickerSites)
            {
                nearMachine = nearMachine || Horizontal(machine, playerFeet) < 7.0f;
            }
            if (m_flickerCooldown <= 0.0f && nearMachine)
            {
                m_flickerTime = 0.0f;
                std::cout << "Unease: vending machines flicker\n";
            }
            return;
        }

        // ~1.6 s of stuttering: brief dropouts that get longer, then a full
        // second of darkness before the light (and hum) comes back.
        m_flickerTime += deltaSeconds;
        const float t = m_flickerTime;
        bool on = true;
        if (t < 1.6f)
        {
            const float chop = std::sin(t * 37.0f) + std::sin(t * 23.0f + 1.3f);
            on = chop > -0.4f + t * 0.5f;
        }
        else if (t < 2.6f)
        {
            on = false;
        }
        else
        {
            m_flickerTime = -1.0f;
            std::uniform_real_distribution<float> wait(40.0f, 70.0f);
            m_flickerCooldown = wait(m_random);
        }

        m_flickerScreen->emissiveFactor = on ? m_flickerEmission : glm::vec3{ 0.0f };
        if (m_level)
        {
            m_level->SetGroupGain("vending", on ? 1.0f : 0.15f);
        }
    }

    void UneaseDirector::Update(
        float deltaSeconds,
        const Atom::Camera& camera,
        const glm::vec3& playerFeet,
        AudioScape& audio
    )
    {
        if (!m_enabled)
        {
            m_staticLevel = 0.0f;
            audio.SetStaticLevel(0.0f);
            if (m_level)
            {
                m_level->SetGroupGain("vending", 1.0f);
            }
            return;
        }

        UpdateFigure(deltaSeconds, camera, playerFeet);
        UpdateFlicker(deltaSeconds, playerFeet, audio);
        audio.SetStaticLevel(m_staticLevel);
    }

    void UneaseDirector::Submit(Atom::Renderer& renderer) const
    {
        if (!m_enabled || !m_figureVisible || !m_figureMesh)
        {
            return;
        }
        glm::mat4 transform = glm::translate(glm::mat4{ 1.0f }, m_figurePosition);
        transform = glm::rotate(transform, m_figureYaw, glm::vec3{ 0.0f, 1.0f, 0.0f });
        renderer.Submit(*m_figureMesh, m_figureMaterial, transform);
    }
}
