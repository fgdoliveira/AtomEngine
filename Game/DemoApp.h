#pragma once

#include "Assets/Model.h"
#include "Core/Application.h"
#include "PlayerController.h"
#include "Renderer/Material.h"
#include "Renderer/Mesh.h"
#include "Scene/Camera.h"

#include <glm/mat4x4.hpp>

#include <memory>
#include <vector>

namespace AtomGame
{
    class DemoApp final : public Atom::Application
    {
    protected:
        bool OnInitialize() override;
        void OnUpdate(float deltaSeconds) override;
        void OnShutdown() override;

    private:
        struct Placement
        {
            const Atom::Model* model = nullptr;
            glm::mat4 transform{ 1.0f };
        };

        bool LoadKit();
        void BuildVignette();
        void UpdateMouseCapture();

        std::unique_ptr<Atom::Mesh> m_groundMesh;
        Atom::Material m_groundMaterial;

        std::unique_ptr<Atom::Model> m_machiya;
        std::unique_ptr<Atom::Model> m_utilityPole;
        std::unique_ptr<Atom::Model> m_vendingMachine;
        std::unique_ptr<Atom::Model> m_torii;
        std::unique_ptr<Atom::Model> m_stoneWall;
        std::unique_ptr<Atom::Model> m_woodFence;
        std::unique_ptr<Atom::Model> m_road;

        std::vector<Placement> m_placements;

        Atom::Camera m_camera;
        PlayerController m_player;
    };
}
