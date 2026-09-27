#include "DemoApp.h"

#include <SDL3/SDL.h>

#include <glm/geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <cstdint>
#include <vector>

namespace AtomGame
{
    namespace
    {
        struct MeshData
        {
            std::vector<Atom::Vertex> vertices;
            std::vector<std::uint32_t> indices;
        };

        // Axis-aligned box centred on the origin, counter-clockwise faces
        // when seen from outside.
        MeshData MakeBox(const glm::vec3& halfExtents, const glm::vec3& color)
        {
            struct Face
            {
                glm::vec3 normal;
                glm::vec3 up;
            };

            constexpr Face faces[] = {
                { { 1, 0, 0 }, { 0, 1, 0 } },
                { { -1, 0, 0 }, { 0, 1, 0 } },
                { { 0, 0, 1 }, { 0, 1, 0 } },
                { { 0, 0, -1 }, { 0, 1, 0 } },
                { { 0, 1, 0 }, { 0, 0, -1 } },
                { { 0, -1, 0 }, { 0, 0, 1 } },
            };

            MeshData data;
            for (const Face& face : faces)
            {
                // right x up == normal, so this corner order is CCW.
                const glm::vec3 right = glm::cross(face.up, face.normal);
                const auto base =
                    static_cast<std::uint32_t>(data.vertices.size());

                const glm::vec2 corners[] = {
                    { -1, -1 }, { 1, -1 }, { 1, 1 }, { -1, 1 }
                };
                for (const glm::vec2& corner : corners)
                {
                    const glm::vec3 position =
                        face.normal + right * corner.x + face.up * corner.y;
                    data.vertices.push_back(Atom::Vertex{
                        position * halfExtents,
                        face.normal,
                        color
                    });
                }

                data.indices.insert(
                    data.indices.end(),
                    { base, base + 1, base + 2, base, base + 2, base + 3 }
                );
            }

            return data;
        }
    }

    bool DemoApp::OnInitialize()
    {
        Atom::Renderer& renderer = GetRenderer();

        const MeshData ground =
            MakeBox({ 20.0f, 0.05f, 20.0f }, { 0.35f, 0.33f, 0.30f });
        const MeshData cube =
            MakeBox({ 0.5f, 0.5f, 0.5f }, { 0.55f, 0.42f, 0.30f });

        m_groundMesh = renderer.CreateMesh(ground.vertices, ground.indices);
        m_cubeMesh = renderer.CreateMesh(cube.vertices, cube.indices);
        if (!m_groundMesh || !m_cubeMesh)
        {
            return false;
        }

        m_player.SetFeetPosition(glm::vec3{ 0.0f, 0.0f, 6.0f });

        return GetInput().SetMouseCaptured(GetWindow().GetSDLWindow(), true);
    }

    void DemoApp::OnUpdate(float deltaSeconds)
    {
        UpdateMouseCapture();

        m_player.Update(GetInput(), m_camera, deltaSeconds);

        Atom::Renderer& renderer = GetRenderer();
        renderer.SetCamera(
            m_camera.GetViewMatrix(),
            m_camera.verticalFov,
            m_camera.nearPlane,
            m_camera.farPlane
        );

        renderer.Submit(
            *m_groundMesh,
            glm::translate(glm::mat4{ 1.0f }, glm::vec3{ 0.0f, -0.05f, 0.0f })
        );

        // A loose grid of 1 m crates as a scale reference while walking.
        for (int x = -3; x <= 3; ++x)
        {
            for (int z = -3; z <= 3; ++z)
            {
                if ((x + z) % 2 != 0 || (x == 0 && z == 0))
                {
                    continue;
                }

                glm::mat4 model = glm::translate(
                    glm::mat4{ 1.0f },
                    glm::vec3{ x * 4.0f, 0.5f, z * 4.0f }
                );
                model = glm::rotate(
                    model,
                    static_cast<float>(x * 7 + z * 3) * 0.3f,
                    glm::vec3{ 0.0f, 1.0f, 0.0f }
                );
                renderer.Submit(*m_cubeMesh, model);
            }
        }
    }

    void DemoApp::OnShutdown()
    {
        m_cubeMesh.reset();
        m_groundMesh.reset();
    }

    void DemoApp::UpdateMouseCapture()
    {
        Atom::Input& input = GetInput();

        // Escape releases the mouse; clicking back in recaptures it.
        if (input.WasKeyPressed(SDL_SCANCODE_ESCAPE))
        {
            if (input.IsMouseCaptured())
            {
                input.SetMouseCaptured(GetWindow().GetSDLWindow(), false);
            }
            else
            {
                RequestQuit();
            }
        }
        else if (!input.IsMouseCaptured()
            && (SDL_GetMouseState(nullptr, nullptr) & SDL_BUTTON_LMASK))
        {
            input.SetMouseCaptured(GetWindow().GetSDLWindow(), true);
        }
    }
}
