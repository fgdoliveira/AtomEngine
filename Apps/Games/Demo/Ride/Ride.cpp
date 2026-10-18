#include "Ride/Ride.h"

#include <glm/geometric.hpp>

#include <algorithm>
#include <cmath>

namespace Demo
{
    namespace
    {
        // Uniform Catmull-Rom between p1 and p2.
        glm::vec3 CatmullRom(const glm::vec3& p0, const glm::vec3& p1, const glm::vec3& p2, const glm::vec3& p3, float t)
        {
            const float t2 = t * t;
            const float t3 = t2 * t;
            return 0.5f * ((2.0f * p1) + (-p0 + p2) * t + (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t2
                           + (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t3);
        }

        float WrapAngle(float a)
        {
            constexpr float Pi = 3.14159265f;
            while (a > Pi) a -= 2.0f * Pi;
            while (a < -Pi) a += 2.0f * Pi;
            return a;
        }

        // Frame-rate independent approach: the share of the gap closed in dt.
        float Approach(float dt, float timeConstant) { return 1.0f - std::exp(-dt / timeConstant); }
    }

    RoadPath::RoadPath(const std::vector<glm::vec3>& points, int samplesPerSegment)
    {
        if (points.size() < 2)
        {
            return;
        }
        const int n = static_cast<int>(points.size());
        const auto at = [&](int i) { return points[static_cast<std::size_t>(std::clamp(i, 0, n - 1))]; };
        for (int i = 0; i + 1 < n; ++i)
        {
            for (int k = 0; k < samplesPerSegment; ++k)
            {
                const float t = static_cast<float>(k) / static_cast<float>(samplesPerSegment);
                m_samples.push_back(CatmullRom(at(i - 1), at(i), at(i + 1), at(i + 2), t));
            }
        }
        m_samples.push_back(points.back());
        m_length.push_back(0.0f);
        for (std::size_t i = 1; i < m_samples.size(); ++i)
        {
            m_length.push_back(m_length.back() + glm::length(m_samples[i] - m_samples[i - 1]));
        }
    }

    std::size_t RoadPath::SegmentAt(float distance) const
    {
        // The last sample at or before `distance` (never the final one).
        const auto it = std::upper_bound(m_length.begin(), m_length.end(), distance);
        const std::size_t index = it == m_length.begin() ? 0 : static_cast<std::size_t>(it - m_length.begin()) - 1;
        return std::min(index, m_samples.size() - 2);
    }

    glm::vec3 RoadPath::PointAt(float distance) const
    {
        if (m_samples.size() < 2)
        {
            return m_samples.empty() ? glm::vec3{ 0.0f } : m_samples.front();
        }
        distance = std::clamp(distance, 0.0f, Length());
        const std::size_t i = SegmentAt(distance);
        const float span = m_length[i + 1] - m_length[i];
        const float t = span > 0.0f ? (distance - m_length[i]) / span : 0.0f;
        return m_samples[i] + (m_samples[i + 1] - m_samples[i]) * t;
    }

    glm::vec3 RoadPath::TangentAt(float distance) const
    {
        if (m_samples.size() < 2)
        {
            return { 0.0f, 0.0f, 1.0f };
        }
        const std::size_t i = SegmentAt(std::clamp(distance, 0.0f, Length()));
        const glm::vec3 d = m_samples[i + 1] - m_samples[i];
        const float length = glm::length(d);
        return length > 0.0f ? d / length : glm::vec3{ 0.0f, 0.0f, 1.0f };
    }

    void RideController::Start(const RoadPath& road, const RideSettings& settings)
    {
        m_road = &road;
        m_settings = settings;
        m_state = {};
        m_travelled = 0.0f;
        m_time = 0.0f;
        m_lateralSpeed = 0.0f;
        Place(0.0f);
    }

    void RideController::SetProgress(float progress)
    {
        if (!m_road)
        {
            return;
        }
        m_travelled = std::clamp(progress, 0.0f, 1.0f) * m_road->Length();
        m_state.speed = std::max(m_state.speed, m_settings.speed); // at cruising, not easing up again
        Place(0.0f);
    }

    const RideState& RideController::Update(float dt, const RideInput& input)
    {
        if (!m_road || m_state.done || dt <= 0.0f)
        {
            return m_state;
        }
        m_time += dt;

        // Speed: cinematic eases up to cruising; steering nudges it, within
        // a range - it never stops.
        float target = m_settings.speed;
        if (m_settings.style == RideStyle::Steer)
        {
            const float throttle = std::clamp(input.throttle, -1.0f, 1.0f);
            const float share = throttle >= 0.0f ? 1.0f + (SpeedRange[1] - 1.0f) * throttle
                                                 : 1.0f + (1.0f - SpeedRange[0]) * throttle;
            target *= share;
        }
        m_state.speed += (target - m_state.speed) * Approach(dt, EaseInSeconds / 3.0f);
        m_travelled += m_state.speed * dt;

        // Across the road.
        if (m_settings.style == RideStyle::Cinematic)
        {
            const float sway = SwayMetres * std::sin(m_time * 0.45f) * std::min(1.0f, m_time / EaseInSeconds);
            m_lateralSpeed = (sway - m_state.offset) / dt;
            m_state.offset = sway;
        }
        else
        {
            const float wanted = std::clamp(input.steer, -1.0f, 1.0f) * m_settings.steerRate;
            m_lateralSpeed += (wanted - m_lateralSpeed) * Approach(dt, 0.25f);
            m_state.offset += m_lateralSpeed * dt;
            // The soft leash (DRIFT's): past the edge it pulls back, and
            // never lets go further than a third beyond.
            const float width = m_settings.steerWidth;
            if (std::abs(m_state.offset) > width)
            {
                const float edge = std::copysign(width, m_state.offset);
                m_state.offset += (edge - m_state.offset) * Approach(dt, 0.2f);
                m_lateralSpeed *= 0.5f;
            }
            m_state.offset = std::clamp(m_state.offset, -width * 1.33f, width * 1.33f);
        }

        Place(dt);
        return m_state;
    }

    void RideController::Place(float dt)
    {
        const float length = m_road->Length();
        const float along = std::min(m_travelled, length);
        const float distance = m_settings.reverse ? length - along : along;
        glm::vec3 forward = m_road->TangentAt(distance);
        if (m_settings.reverse)
        {
            forward = -forward;
        }
        forward.y = 0.0f;
        forward = glm::length(forward) > 0.0f ? glm::normalize(forward) : glm::vec3{ 0.0f, 0.0f, 1.0f };
        const glm::vec3 right = glm::cross(forward, glm::vec3{ 0.0f, 1.0f, 0.0f });

        const float heading = std::atan2(forward.x, forward.z);
        // Lean into the turn: the bank that balances it (atan(v * yawRate / g)),
        // plus a little from moving across the road; eased so it never snaps.
        // The turn is measured over 4 m of road: the sampled road's heading
        // steps at each sample, so frame-to-frame it would only spike.
        if (dt > 0.0f)
        {
            const auto headingAt = [&](float d) {
                const glm::vec3 t = m_road->TangentAt(d) * (m_settings.reverse ? -1.0f : 1.0f);
                return std::atan2(t.x, t.z);
            };
            constexpr float Span = 2.0f;
            const float ahead = m_settings.reverse ? distance - Span : distance + Span;
            const float behind = m_settings.reverse ? distance + Span : distance - Span;
            const float curvature = WrapAngle(headingAt(ahead) - headingAt(behind)) / (2.0f * Span); // rad/m
            const float yawRate = curvature * m_state.speed;
            const float wanted = std::clamp(std::atan(m_state.speed * -yawRate / 9.81f) + 0.06f * m_lateralSpeed,
                                            -MaxLean, MaxLean);
            m_state.lean += (wanted - m_state.lean) * Approach(dt, 0.3f);
        }
        m_state.heading = heading;
        m_state.position = m_road->PointAt(distance) + right * m_state.offset;
        m_state.progress = length > 0.0f ? along / length : 1.0f;
        m_state.done = m_state.done || m_travelled >= length;
    }
}
