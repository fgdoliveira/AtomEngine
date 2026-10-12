#pragma once

#include "Renderer/Material.h"

#include <vector>
#include "Level/LevelData.h"
#include "Renderer/Mesh.h"

#include <glm/vec3.hpp>

#include <memory>
#include <random>

namespace Atom
{
    class Camera;
    class Renderer;
}

namespace AtomFramework
{
    class Level; // v0.0.14: the framework's
}

namespace Demo
{
    using namespace AtomFramework; // v0.0.14: the world layer (levels, world, interaction) lives there

    class AudioScape;

    // Quiet "is something there?" moments, never a jump scare:
    //  - a dark figure stands far off in the fog and is gone once you get
    //    close; it returns later somewhere you aren't looking,
    //  - radio static rises the nearer you are to it,
    //  - vending machines occasionally flicker and their hum stutters.
    class UneaseDirector
    {
    public:
        bool Initialize(Atom::Renderer& renderer);
        // Per level: where the figure may stand, what flickers.
        void Configure(const LevelUnease& config, Level* level);
        void Shutdown();

        void Update(
            float deltaSeconds,
            const Atom::Camera& camera,
            const glm::vec3& playerFeet,
            AudioScape& audio
        );
        void Submit(Atom::Renderer& renderer) const;

        void SetEnabled(bool enabled);
        bool IsEnabled() const { return m_enabled; }
        bool IsFigureVisible() const { return m_figureVisible; }

    private:
        void UpdateFigure(float deltaSeconds, const Atom::Camera& camera, const glm::vec3& playerFeet);
        void UpdateFlicker(float deltaSeconds, const glm::vec3& playerFeet, AudioScape& audio);
        bool PlaceFigure(const Atom::Camera& camera, const glm::vec3& playerFeet);

        std::unique_ptr<Atom::Mesh> m_figureMesh;
        Atom::Material m_figureMaterial;

        glm::vec3 m_figurePosition{ 0.0f };
        float m_figureYaw = 0.0f;
        bool m_figureVisible = false;
        float m_figureCooldown = 8.0f;
        float m_staticLevel = 0.0f;

        LevelUnease m_config;
        Level* m_level = nullptr;
        std::vector<Atom::Material*> m_flickerScreens; // one per model using it
        glm::vec3 m_flickerEmission{ 0.0f };
        float m_flickerCooldown = 20.0f;
        float m_flickerTime = -1.0f; // < 0 when not flickering

        bool m_enabled = true;
        std::mt19937 m_random{ 4242 };
    };
}
