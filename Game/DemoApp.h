#pragma once

#include "Assets/Model.h"
#include "Core/Application.h"
#include "Physics/CollisionWorld.h"
#include "PlayerController.h"
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
        void UpdateWindowTitle(float deltaSeconds);

        std::unique_ptr<Atom::Model> m_street;
        Atom::CollisionWorld m_collision;

        Atom::Camera m_camera;
        PlayerController m_player;

        float m_titleTimer = 0.0f;
        int m_titleFrames = 0;
    };
}
