#include "Assets/Animation.h"

#include <glm/geometric.hpp>
#include <glm/gtc/quaternion.hpp>

#include <algorithm>

namespace Atom
{
    namespace
    {
        glm::quat ToQuat(const glm::vec4& v)
        {
            return glm::quat(v.w, v.x, v.y, v.z); // glm: (w, x, y, z)
        }

        glm::vec4 FromQuat(const glm::quat& q)
        {
            return glm::vec4{ q.x, q.y, q.z, q.w };
        }
    }

    glm::vec4 SampleChannel(const AnimationChannel& channel, float time)
    {
        const std::vector<float>& times = channel.times;
        if (times.empty())
        {
            return glm::vec4{ 0.0f };
        }
        const bool cubic = channel.interpolation == Interpolation::CubicSpline;
        const auto value = [&](std::size_t key) {
            return channel.values[cubic ? key * 3 + 1 : key];
        };
        const bool rotation = channel.path == AnimationPath::Rotation;
        const auto finish = [&](glm::vec4 v) {
            return rotation ? FromQuat(glm::normalize(ToQuat(v))) : v;
        };

        if (time <= times.front())
        {
            return finish(value(0));
        }
        if (time >= times.back())
        {
            return finish(value(times.size() - 1));
        }

        // The key interval containing `time`.
        const std::size_t next = static_cast<std::size_t>(
            std::upper_bound(times.begin(), times.end(), time) - times.begin());
        const std::size_t previous = next - 1;
        const float span = times[next] - times[previous];
        const float t = span > 0.0f ? (time - times[previous]) / span : 0.0f;

        switch (channel.interpolation)
        {
        case Interpolation::Step:
            return finish(value(previous));
        case Interpolation::CubicSpline:
        {
            // Hermite: p0, out-tangent of p0, p1, in-tangent of p1.
            const glm::vec4 p0 = value(previous);
            const glm::vec4 m0 = channel.values[previous * 3 + 2] * span;
            const glm::vec4 p1 = value(next);
            const glm::vec4 m1 = channel.values[next * 3] * span;
            const float t2 = t * t;
            const float t3 = t2 * t;
            return finish((2 * t3 - 3 * t2 + 1) * p0 + (t3 - 2 * t2 + t) * m0
                + (-2 * t3 + 3 * t2) * p1 + (t3 - t2) * m1);
        }
        case Interpolation::Linear:
        default:
            if (rotation)
            {
                return FromQuat(glm::slerp(ToQuat(value(previous)), ToQuat(value(next)), t));
            }
            return value(previous) + (value(next) - value(previous)) * t;
        }
    }
}
