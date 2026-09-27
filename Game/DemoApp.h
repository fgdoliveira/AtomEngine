#pragma once

#include "Core/Application.h"
#include "PlayerController.h"
#include "Renderer/Mesh.h"
#include "Scene/Camera.h"

#include <memory>

namespace AtomGame
{
    class DemoApp final : public Atom::Application
    {
    protected:
        bool OnInitialize() override;
        void OnUpdate(float deltaSeconds) override;
        void OnShutdown() override;

    private:
        void UpdateMouseCapture();

        std::unique_ptr<Atom::Mesh> m_groundMesh;
        std::unique_ptr<Atom::Mesh> m_cubeMesh;

        Atom::Camera m_camera;
        PlayerController m_player;
    };
}
