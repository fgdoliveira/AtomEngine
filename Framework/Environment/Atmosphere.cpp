#include "Environment/Atmosphere.h"

#include "Renderer/Renderer.h"

#include <glm/common.hpp>
#include <glm/gtc/constants.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace AtomFramework
{
    namespace
    {
        constexpr int CellSize = 64;
        constexpr int AtlasColumns = 3; // 0: fog puff, 1: leaf, 2: streak
        constexpr float FogCell = 0.0f;
        constexpr float LeafCell = 1.0f;
        constexpr float StreakCell = 2.0f;

        // Rain (M50): drops in a box that follows the player, as many as the
        // rain asks for. Close drops are what reads; far ones are the fog.
        constexpr int DropCount = 1400;
        constexpr float DropHalfExtent = 9.0f;
        constexpr float DropCeiling = 9.0f;  // above the feet
        constexpr float DropFloor = -0.2f;   // below the feet, where they end
        constexpr float DropLength = 0.45f;  // streak, metres
        constexpr float DropWidth = 0.018f;

        constexpr int FlakeCount = 160;
        constexpr float FlakeHalfExtent = 16.0f; // metres around the player
        constexpr float FlakeCeiling = 9.0f;

        constexpr int BankCount = 36;

        // Dust (M45): a small box of motes around the player, wrapping like
        // the leaves; only the beam shows them, so they needn't reach far.
        constexpr int MoteCount = 280;
        constexpr float MoteHalfExtent = 4.0f;
        constexpr float MoteHeight = 2.6f;
        constexpr float BankRadius = 32.0f;

        float Hash(int x, int y)
        {
            std::uint32_t h = static_cast<std::uint32_t>(x) * 374761393u
                + static_cast<std::uint32_t>(y) * 668265263u;
            h = (h ^ (h >> 13)) * 1274126177u;
            return static_cast<float>(h & 0xFFFF) / 65535.0f;
        }

        // Smooth value noise over a 64-texel cell, for a lumpy fog puff.
        float CellNoise(float x, float y, int scale)
        {
            const float fx = x * scale;
            const float fy = y * scale;
            const int ix = static_cast<int>(std::floor(fx));
            const int iy = static_cast<int>(std::floor(fy));
            const float tx = fx - ix;
            const float ty = fy - iy;
            const float sx = tx * tx * (3 - 2 * tx);
            const float sy = ty * ty * (3 - 2 * ty);
            const float a = Hash(ix, iy), b = Hash(ix + 1, iy);
            const float c = Hash(ix, iy + 1), d = Hash(ix + 1, iy + 1);
            return (a + (b - a) * sx) + ((c + (d - c) * sx) - (a + (b - a) * sx)) * sy;
        }

        // White RGB with the shape in alpha; particles tint it.
        std::vector<std::uint8_t> BuildAtlas()
        {
            const int width = CellSize * AtlasColumns;
            std::vector<std::uint8_t> pixels(width * CellSize * 4, 255);

            for (int y = 0; y < CellSize; ++y)
            {
                for (int x = 0; x < CellSize; ++x)
                {
                    const float u = (x + 0.5f) / CellSize * 2.0f - 1.0f;
                    const float v = (y + 0.5f) / CellSize * 2.0f - 1.0f;

                    // Fog puff: soft radial falloff broken up by noise,
                    // reaching zero well inside the cell edge.
                    const float radius = std::sqrt(u * u + v * v);
                    const float lumps = 0.6f + 0.4f * CellNoise(u + 1, v + 1, 3);
                    float puff = std::clamp(1.0f - radius / 0.95f, 0.0f, 1.0f);
                    puff = puff * puff * (3 - 2 * puff) * lumps;

                    // Leaf: a pointed ellipse with a darker midrib.
                    const float leafShape = (u * u) / 0.25f + (v * v) / 0.9f
                        + 0.35f * std::abs(v) * (1.0f - std::abs(u));
                    float leaf = std::clamp((1.0f - leafShape) * 6.0f, 0.0f, 1.0f);
                    const float rib = std::abs(u) < 0.05f ? 0.6f : 1.0f;

                    std::uint8_t* fog = &pixels[(y * width + x) * 4];
                    fog[3] = static_cast<std::uint8_t>(puff * 255.0f);

                    // Streak: a soft vertical line, thin, fading at both ends.
                    const float across = std::clamp(1.0f - std::abs(u), 0.0f, 1.0f);
                    const float along = std::clamp(1.0f - v * v * v * v, 0.0f, 1.0f);
                    std::uint8_t* streak = &pixels[(y * width + x + 2 * CellSize) * 4];
                    streak[3] = static_cast<std::uint8_t>(across * across * along * 255.0f);

                    std::uint8_t* leafPixel = &pixels[(y * width + x + CellSize) * 4];
                    const auto shade = static_cast<std::uint8_t>(255.0f * rib);
                    leafPixel[0] = shade;
                    leafPixel[1] = shade;
                    leafPixel[2] = shade;
                    leafPixel[3] = static_cast<std::uint8_t>(leaf * 255.0f);
                }
            }
            return pixels;
        }
    }

    bool Atmosphere::Initialize(Atom::Renderer& renderer)
    {
        const std::vector<std::uint8_t> pixels = BuildAtlas();
        m_atlas = renderer.CreateTexture(
            CellSize * AtlasColumns, CellSize, pixels.data(), true);
        if (!m_atlas)
        {
            return false;
        }
        renderer.SetParticleAtlas(m_atlas.get(), AtlasColumns);

        m_flakes.resize(FlakeCount);
        m_banks.resize(BankCount);
        m_particles.reserve(FlakeCount + BankCount + MoteCount + DropCount);
        return true;
    }

    void Atmosphere::Shutdown()
    {
        m_atlas.reset();
    }

    void Atmosphere::RespawnBank(Bank& bank, const glm::vec3& center, bool anywhere)
    {
        std::uniform_real_distribution<float> unit(0.0f, 1.0f);
        const float angle = glm::two_pi<float>() * unit(m_random);
        // New banks appear upwind at the edge, older ones drift through.
        const float distance = anywhere
            ? BankRadius * std::sqrt(unit(m_random))
            : BankRadius * (0.7f + 0.3f * unit(m_random));
        bank.size = 5.0f + 4.0f * unit(m_random);
        bank.position = center + glm::vec3{
            std::cos(angle) * distance,
            0.0f,
            std::sin(angle) * distance
        };
        // Keep the puff's soft edge, not its middle, near the ground.
        bank.position.y = bank.size * 0.32f + 0.3f * unit(m_random);
        bank.age = anywhere ? unit(m_random) * 10.0f : 0.0f;
        bank.lifetime = 14.0f + 10.0f * unit(m_random);
        bank.opacity = 0.16f + 0.12f * unit(m_random);
    }

    void Atmosphere::Update(
        float deltaSeconds,
        const glm::vec3& center,
        const glm::vec3& fogColor,
        float fogStrength
    )
    {
        m_time += deltaSeconds;
        std::uniform_real_distribution<float> unit(0.0f, 1.0f);

        if (!m_seeded)
        {
            for (Flake& flake : m_flakes)
            {
                flake.position = center + glm::vec3{
                    (unit(m_random) * 2 - 1) * FlakeHalfExtent,
                    unit(m_random) * FlakeCeiling,
                    (unit(m_random) * 2 - 1) * FlakeHalfExtent
                };
                flake.size = 0.07f + 0.07f * unit(m_random);
                flake.fallSpeed = 0.35f + 0.35f * unit(m_random);
                flake.phase = glm::two_pi<float>() * unit(m_random);
                flake.spin = (unit(m_random) * 2 - 1) * 3.0f;
                flake.rotation = glm::two_pi<float>() * unit(m_random);
                flake.shade = unit(m_random);
            }
            for (Bank& bank : m_banks)
            {
                RespawnBank(bank, center, true);
            }
            m_motes.resize(MoteCount);
            for (Mote& mote : m_motes)
            {
                mote.position = center + glm::vec3{
                    (unit(m_random) * 2 - 1) * MoteHalfExtent,
                    unit(m_random) * MoteHeight,
                    (unit(m_random) * 2 - 1) * MoteHalfExtent };
                mote.size = 0.02f + 0.025f * unit(m_random);
                mote.phase = glm::two_pi<float>() * unit(m_random);
            }
            m_drops.resize(DropCount);
            for (Drop& drop : m_drops)
            {
                drop.position = center + glm::vec3{
                    (unit(m_random) * 2 - 1) * DropHalfExtent,
                    DropFloor + unit(m_random) * (DropCeiling - DropFloor),
                    (unit(m_random) * 2 - 1) * DropHalfExtent };
                drop.speed = 7.0f + 2.0f * unit(m_random);
            }
            m_seeded = true;
        }

        // Gusts: the wind breathes rather than blowing steadily.
        const float gust = 0.6f + 0.4f * std::sin(m_time * 0.37f) * std::sin(m_time * 0.11f + 1.0f);
        const glm::vec3 wind = m_wind * (0.5f + gust);
        m_currentWind = wind;

        m_particles.clear();

        for (Flake& flake : m_flakes)
        {
            if (!m_leaves)
            {
                break;
            }
            // Flutter: side-to-side sway while tumbling down.
            const float sway = std::sin(m_time * 1.7f + flake.phase);
            flake.position += (wind + glm::vec3{ sway * 0.35f, -flake.fallSpeed, 0.0f })
                * deltaSeconds;
            flake.rotation += flake.spin * deltaSeconds;

            // Wrap around the box that follows the player.
            glm::vec3 offset = flake.position - center;
            for (int axis : { 0, 2 })
            {
                if (offset[axis] > FlakeHalfExtent) { offset[axis] -= 2 * FlakeHalfExtent; }
                if (offset[axis] < -FlakeHalfExtent) { offset[axis] += 2 * FlakeHalfExtent; }
            }
            if (flake.position.y < 0.02f)
            {
                offset.y = FlakeCeiling;
            }
            else
            {
                offset.y = flake.position.y - center.y;
            }
            flake.position = center + offset;

            // Mix of dead leaves (brown) and ash (grey).
            const glm::vec3 color = glm::mix(
                glm::vec3{ 0.09f, 0.07f, 0.05f },
                glm::vec3{ 0.16f, 0.16f, 0.15f },
                flake.shade
            );
            Atom::Particle particle{};
            particle.position = flake.position;
            particle.size = flake.size;
            particle.color = glm::vec4{ color, 0.9f };
            particle.rotation = flake.rotation;
            particle.atlasCell = LeafCell;
            m_particles.push_back(particle);
        }

        for (Mote& mote : m_motes)
        {
            if (!m_dust)
            {
                break;
            }
            // Still air: each mote wanders on slow sines of its own and
            // settles a little, then wraps round the box like the leaves.
            const float t = m_time * 0.25f + mote.phase;
            mote.position += glm::vec3{ std::sin(t * 1.3f) * 0.05f, std::sin(t * 0.7f) * 0.03f - 0.01f,
                                        std::cos(t * 1.1f) * 0.05f } * deltaSeconds;
            glm::vec3 offset = mote.position - center;
            for (int axis : { 0, 2 })
            {
                if (offset[axis] > MoteHalfExtent) { offset[axis] -= 2 * MoteHalfExtent; }
                if (offset[axis] < -MoteHalfExtent) { offset[axis] += 2 * MoteHalfExtent; }
            }
            if (offset.y < 0.0f) { offset.y += MoteHeight; }
            if (offset.y > MoteHeight) { offset.y -= MoteHeight; }
            mote.position = center + offset;

            Atom::Particle particle{};
            particle.position = mote.position;
            particle.size = mote.size;
            particle.color = glm::vec4{ 0.9f, 0.86f, 0.78f, 0.6f };
            particle.rotation = mote.phase;
            particle.atlasCell = FogCell; // a soft round puff, tiny
            particle.beamLit = 1.0f;      // seen only in the flashlight's beam
            m_particles.push_back(particle);
        }

        // Rain: the first `rain` share of the drops, falling with the wind.
        const int drops = static_cast<int>(std::round(std::clamp(m_rain, 0.0f, 1.0f) * DropCount));
        for (int i = 0; i < drops; ++i)
        {
            Drop& drop = m_drops[i];
            drop.position += (m_wind * 1.6f - glm::vec3{ 0.0f, drop.speed, 0.0f }) * deltaSeconds;
            glm::vec3 offset = drop.position - center;
            for (int axis : { 0, 2 })
            {
                if (offset[axis] > DropHalfExtent) { offset[axis] -= 2 * DropHalfExtent; }
                if (offset[axis] < -DropHalfExtent) { offset[axis] += 2 * DropHalfExtent; }
            }
            if (offset.y < DropFloor) { offset.y += DropCeiling - DropFloor; }
            if (offset.y > DropCeiling) { offset.y -= DropCeiling - DropFloor; } // the player dropped
            drop.position = center + offset;

            Atom::Particle particle{};
            particle.position = drop.position;
            particle.size = DropWidth;
            particle.color = glm::vec4{ m_rainColor, 0.45f };
            particle.atlasCell = StreakCell;
            particle.stretch = DropLength;
            m_particles.push_back(particle);
        }

        for (Bank& bank : m_banks)
        {
            if (!m_fogBanks)
            {
                break;
            }
            bank.age += deltaSeconds;
            bank.position += wind * 0.6f * deltaSeconds;

            glm::vec3 offset = bank.position - center;
            offset.y = 0.0f;
            if (bank.age > bank.lifetime
                || glm::dot(offset, offset) > BankRadius * BankRadius * 1.3f)
            {
                RespawnBank(bank, center, false);
            }

            // Fade in and out over the bank's life so none pop.
            const float t = bank.age / bank.lifetime;
            const float fade = std::clamp(std::min(t, 1.0f - t) * 4.0f, 0.0f, 1.0f);
            const float alpha = bank.opacity * fade * fogStrength;
            if (alpha <= 0.002f)
            {
                continue;
            }

            Atom::Particle particle{};
            particle.position = bank.position;
            particle.size = bank.size;
            // Slightly brighter than the fog so the banks read as wisps
            // against the darker road and walls instead of vanishing into it.
            particle.color = glm::vec4{ fogColor * 1.3f, alpha };
            particle.rotation = bank.lifetime; // fixed, varied per bank
            particle.atlasCell = FogCell;
            m_particles.push_back(particle);
        }
    }

    glm::vec3 Atmosphere::GetRainDirection() const
    {
        return glm::normalize(m_wind * 1.6f - glm::vec3{ 0.0f, 8.0f, 0.0f });
    }

    void Atmosphere::Submit(Atom::Renderer& renderer) const
    {
        if (m_enabled)
        {
            renderer.SetParticleStreak(GetRainDirection());
            renderer.SubmitParticles(m_particles);
        }
    }
}
