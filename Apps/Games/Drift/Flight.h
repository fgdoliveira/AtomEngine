#pragma once

// DRIFT's flight (M77): the path, the ship and its spring camera, ported
// line by line from the three.js original (src/world.js `path`,
// src/ship.js `Ship.update`) with every constant kept. Pure: no renderer,
// no SDL - unit-tested against the original's formulas.

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include <cstdint>
#include <random>

namespace Drift
{
    // The flight path's centre at depth z (the ship flies toward -Z).
    glm::vec2 Path(float z);

    // Lateral play area around the path (the "soft leash").
    inline constexpr float Bound = 11.0f;

    // This frame's controls, already combined: x right, y up, each about
    // -1..1 (keys) plus the mouse's 0.35 share.
    struct ShipInput
    {
        float x = 0.0f;
        float y = 0.0f;
        bool boost = false;
    };

    // What the camera should do this frame.
    struct CameraPose
    {
        glm::vec3 position{ 0.0f };
        glm::vec3 target{ 0.0f }; // looked at
        float roll = 0.0f;        // radians about the view axis
        float fovDegrees = 70.0f;
        glm::mat4 View() const;   // lookAt, then the roll
    };

    class Ship
    {
    public:
        // `seed` drives the camera shake's random directions.
        explicit Ship(std::uint32_t seed = 1);

        // One frame; returns the forward speed (m/s), as the original does.
        float Update(float dt, float flow, const ShipInput& input);

        glm::vec3 position{ 0.0f, 0.0f, 0.0f };
        glm::vec2 velocity{ 0.0f };   // lateral (x, y), m/s
        float speed = 40.0f;          // cruise, before boost
        float boost = 0.0f;           // 0..1, eased
        float shake = 0.0f;           // 0..1, set by a hit (M78), decays
        glm::vec3 modelRotation{ 0.0f }; // Euler XYZ: pitch, yaw, bank
        glm::vec3 modelScale{ 1.0f };    // squash and stretch on boost

        const CameraPose& Camera() const { return m_camera; }
        // The model's transform (position, Euler XYZ rotation, scale).
        glm::mat4 ModelMatrix() const;

    private:
        glm::vec3 m_cameraPosition{ 0.0f, 2.0f, 8.0f };
        CameraPose m_camera;
        std::mt19937 m_random;
    };
}
