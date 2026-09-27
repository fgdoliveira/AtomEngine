#include "DemoApp.h"

#include <SDL3/SDL.h>

#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <cstdint>
#include <string>
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
        // when seen from outside. UVs are in metres.
        MeshData MakeBox(const glm::vec3& halfExtents)
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
                        (face.normal + right * corner.x + face.up * corner.y)
                        * halfExtents;
                    data.vertices.push_back(Atom::Vertex{
                        position,
                        face.normal,
                        glm::vec2{
                            glm::dot(position, right),
                            glm::dot(position, face.up)
                        }
                    });
                }

                data.indices.insert(
                    data.indices.end(),
                    { base, base + 1, base + 2, base, base + 2, base + 3 }
                );
            }

            return data;
        }

        glm::mat4 Place(float x, float z, float yawDegrees = 0.0f)
        {
            const glm::mat4 translation =
                glm::translate(glm::mat4{ 1.0f }, glm::vec3{ x, 0.0f, z });
            return glm::rotate(
                translation,
                glm::radians(yawDegrees),
                glm::vec3{ 0.0f, 1.0f, 0.0f }
            );
        }
    }

    bool DemoApp::OnInitialize()
    {
        Atom::Renderer& renderer = GetRenderer();

        const MeshData ground = MakeBox({ 20.0f, 0.05f, 20.0f });
        m_groundMesh = renderer.CreateMesh(ground.vertices, ground.indices);
        if (!m_groundMesh)
        {
            return false;
        }
        m_groundMaterial.baseColorFactor = glm::vec4{ 0.09f, 0.08f, 0.06f, 1.0f };

        if (!LoadKit())
        {
            return false;
        }
        BuildVignette();

        // Stand at the east end of the street, looking west along it.
        m_player.SetFeetPosition(glm::vec3{ 15.0f, 0.0f, 1.0f });
        m_camera.SetRotation(-glm::half_pi<float>(), 0.0f);

        return GetInput().SetMouseCaptured(GetWindow().GetSDLWindow(), true);
    }

    bool DemoApp::LoadKit()
    {
        const char* basePath = SDL_GetBasePath();
        const std::string kitPath =
            std::string(basePath ? basePath : "") + "Assets/Kit/";

        Atom::Renderer& renderer = GetRenderer();
        const auto load = [&](const char* name) {
            return Atom::Model::Load(renderer, kitPath + name + ".glb");
        };

        m_machiya = load("machiya");
        m_utilityPole = load("utility_pole");
        m_vendingMachine = load("vending_machine");
        m_torii = load("torii");
        m_stoneWall = load("stone_wall");
        m_woodFence = load("wood_fence");
        m_road = load("road");

        return m_machiya && m_utilityPole && m_vendingMachine && m_torii
            && m_stoneWall && m_woodFence && m_road;
    }

    void DemoApp::BuildVignette()
    {
        // A short preview street; the real layout arrives in M6.
        // Kit fronts face +Z, the road runs along X.
        const auto add = [&](const Atom::Model& model, const glm::mat4& transform) {
            m_placements.push_back(Placement{ &model, transform });
        };

        for (float x = -16.0f; x <= 16.0f; x += 8.0f)
        {
            add(*m_road, Place(x, 0.0f));
        }

        // North side: houses facing the road, a vending machine between.
        add(*m_machiya, Place(-8.0f, -8.4f));
        add(*m_machiya, Place(1.0f, -8.4f));
        add(*m_vendingMachine, Place(-3.5f, -4.1f));
        add(*m_utilityPole, Place(-12.0f, -4.0f));
        add(*m_utilityPole, Place(8.0f, -4.0f));

        // South side: shrine wall, torii entrance, then a board fence.
        for (float x : { -14.0f, -10.0f, -6.0f })
        {
            add(*m_stoneWall, Place(x, 4.3f, 180.0f));
        }
        add(*m_torii, Place(-1.8f, 6.0f, 180.0f));
        for (float x : { 2.5f, 6.5f, 10.5f, 14.5f })
        {
            add(*m_woodFence, Place(x, 4.3f, 180.0f));
        }
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

        // Ground top sits just under the road surface to avoid z-fighting.
        renderer.Submit(
            *m_groundMesh,
            m_groundMaterial,
            glm::translate(glm::mat4{ 1.0f }, glm::vec3{ 0.0f, -0.07f, 0.0f })
        );

        for (const Placement& placement : m_placements)
        {
            placement.model->Submit(renderer, placement.transform);
        }
    }

    void DemoApp::OnShutdown()
    {
        m_placements.clear();
        m_road.reset();
        m_woodFence.reset();
        m_stoneWall.reset();
        m_torii.reset();
        m_vendingMachine.reset();
        m_utilityPole.reset();
        m_machiya.reset();
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
