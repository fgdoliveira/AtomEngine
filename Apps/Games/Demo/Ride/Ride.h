#pragma once

// Riding a vehicle along a road (the Kawasaki on the coast road). Pure: no
// renderer, no input devices - the game feeds it each frame and places the
// vehicle and the camera from what it returns. Unit-tested.
//
// Two styles. Cinematic (the first ride): the bike rides itself, easing up
// to speed, swaying a little. Steer (once ridden): the player moves it
// across the road on a soft leash, like DRIFT's ship, and nudges the speed;
// it never stops and never leaves the road.

#include <glm/vec3.hpp>

#include <vector>

namespace Demo
{
    // A smooth road through control points (Catmull-Rom), measured by
    // length so a speed in m/s moves at that speed on every bend.
    class RoadPath
    {
    public:
        RoadPath() = default;
        explicit RoadPath(const std::vector<glm::vec3>& points, int samplesPerSegment = 16);

        float Length() const { return m_length.empty() ? 0.0f : m_length.back(); }
        // Distance 0..Length() along the road (clamped).
        glm::vec3 PointAt(float distance) const;
        glm::vec3 TangentAt(float distance) const; // unit, toward the end

    private:
        std::size_t SegmentAt(float distance) const;

        std::vector<glm::vec3> m_samples;
        std::vector<float> m_length; // cumulative, per sample
    };

    enum class RideStyle
    {
        Cinematic,
        Steer,
    };

    struct RideSettings
    {
        RideStyle style = RideStyle::Cinematic;
        bool reverse = false;     // from the road's end to its start
        float speed = 14.0f;      // m/s, cruising
        float steerWidth = 3.0f;  // metres either side of the centre
        float steerRate = 2.5f;   // lateral m/s at full input
    };

    // This frame's controls (steer style only): -1..1 each.
    struct RideInput
    {
        float steer = 0.0f;    // + right
        float throttle = 0.0f; // + faster
    };

    struct RideState
    {
        glm::vec3 position{ 0.0f };
        float heading = 0.0f;  // radians about +Y; 0 faces +Z
        float lean = 0.0f;     // radians, + leans right (into a right turn)
        float speed = 0.0f;    // m/s
        float offset = 0.0f;   // metres right of the road's centre
        float progress = 0.0f; // 0..1 along the route
        bool done = false;     // the end was reached (stays true)
    };

    class RideController
    {
    public:
        static constexpr float EaseInSeconds = 3.0f; // cinematic: 0 to cruising
        static constexpr float SwayMetres = 0.3f;
        static constexpr float MaxLean = 0.45f;     // radians, about 26 degrees
        static constexpr float SpeedRange[2] = { 0.6f, 1.3f }; // steer: of cruising

        void Start(const RoadPath& road, const RideSettings& settings);
        const RideState& Update(float dt, const RideInput& input = {});
        const RideState& State() const { return m_state; }
        // Jumps along the route (tests and scripted runs): 0..1.
        void SetProgress(float progress);

    private:
        void Place(float dt);

        const RoadPath* m_road = nullptr;
        RideSettings m_settings;
        RideState m_state;
        float m_travelled = 0.0f; // metres along the route
        float m_time = 0.0f;
        float m_lateralSpeed = 0.0f;
    };
}
