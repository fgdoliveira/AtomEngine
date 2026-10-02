#pragma once

#include "Renderer/Particles.h"
#include "Renderer/Texture.h"

#include <glm/vec3.hpp>

#include <memory>
#include <random>
#include <vector>

namespace Atom
{
    class Renderer;
}

namespace AtomGame
{
    // Particle ambience around the player: falling leaves and ash, and slow
    // fog banks drifting at ground level. Particles live in a box that
    // follows the player and wrap around its edges, so the effect is
    // everywhere without simulating the whole street.
    class Atmosphere
    {
    public:
        bool Initialize(Atom::Renderer& renderer);
        void Shutdown();

        // fogColor is linear; fogStrength 0..1 scales the fog banks.
        void Update(
            float deltaSeconds,
            const glm::vec3& center,
            const glm::vec3& fogColor,
            float fogStrength
        );
        void Submit(Atom::Renderer& renderer) const;

        // Per level: which effects belong there (none indoors).
        // Dust (M45): motes hanging in still air, invisible until the
        // flashlight's beam catches them.
        void Configure(bool leaves, bool fogBanks, bool dust = false)
        {
            m_leaves = leaves;
            m_fogBanks = fogBanks;
            m_dust = dust;
        }

        // M50: the wind the gusts breathe around (m/s), and rain: 0..1 of
        // the drops, in a colour lit by the sky.
        void SetWind(const glm::vec3& wind) { m_wind = wind; }
        void SetRain(float rain, const glm::vec3& color)
        {
            m_rain = rain;
            m_rainColor = color;
        }
        // The way the drops fall (normalised), for the streaks.
        glm::vec3 GetRainDirection() const;

        // Current wind including gusts (m/s), for vertex sway.
        const glm::vec3& GetWind() const { return m_currentWind; }

        void SetEnabled(bool enabled) { m_enabled = enabled; }
        bool IsEnabled() const { return m_enabled; }

    private:
        struct Flake
        {
            glm::vec3 position;
            float size;
            float fallSpeed;
            float phase;
            float spin;
            float rotation;
            float shade;
        };

        struct Bank
        {
            glm::vec3 position;
            float size;
            float age;
            float lifetime;
            float opacity;
        };

        struct Drop
        {
            glm::vec3 position;
            float speed; // m/s down
        };

        struct Mote
        {
            glm::vec3 position;
            float size;
            float phase;
        };

        void RespawnBank(Bank& bank, const glm::vec3& center, bool anywhere);

        std::unique_ptr<Atom::Texture> m_atlas;
        std::vector<Flake> m_flakes;
        std::vector<Bank> m_banks;
        std::vector<Mote> m_motes;
        std::vector<Drop> m_drops;
        float m_rain = 0.0f;
        glm::vec3 m_rainColor{ 0.6f };
        std::vector<Atom::Particle> m_particles;

        glm::vec3 m_wind{ 0.45f, 0.0f, 0.15f };
        glm::vec3 m_currentWind{ 0.0f };
        float m_time = 0.0f;
        bool m_enabled = true;
        bool m_leaves = true;
        bool m_fogBanks = true;
        bool m_dust = false;
        bool m_seeded = false;
        std::mt19937 m_random{ 777 };
    };
}
