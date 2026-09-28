#pragma once

#include <glm/vec4.hpp>

#include <string>
#include <vector>

namespace Atom
{
    // Rigid node animation, as glTF stores it: per node, keyframed
    // translation, rotation (quaternion, stored x y z w) or scale. Playing a
    // clip means sampling every channel at time t and redrawing the node's
    // parts with the resulting transform.
    enum class AnimationPath
    {
        Translation,
        Rotation,
        Scale,
    };

    enum class Interpolation
    {
        Linear, // lerp; slerp for rotations
        Step,   // hold the previous key
        CubicSpline,
    };

    struct AnimationChannel
    {
        int node = -1;
        AnimationPath path = AnimationPath::Translation;
        Interpolation interpolation = Interpolation::Linear;
        std::vector<float> times;      // seconds, ascending
        std::vector<glm::vec4> values; // one per key (three for CubicSpline: in, value, out)
    };

    struct AnimationClip
    {
        std::string name;
        float duration = 0.0f; // last key time
        std::vector<AnimationChannel> channels;
    };

    // Value of `channel` at `time`, clamped to its first and last keys.
    // Rotations come back normalised. Cubic splines are evaluated with their
    // tangents (Hermite), as the glTF spec defines.
    glm::vec4 SampleChannel(const AnimationChannel& channel, float time);
}
