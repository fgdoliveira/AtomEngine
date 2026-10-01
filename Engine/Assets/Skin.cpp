#include "Assets/Skin.h"

#include <glm/gtc/matrix_transform.hpp>

namespace Atom
{
    glm::mat4 NodeTransform::ToMatrix() const
    {
        return glm::translate(glm::mat4{ 1.0f }, translation)
            * glm::mat4_cast(rotation)
            * glm::scale(glm::mat4{ 1.0f }, scale);
    }

    void ApplyClip(const AnimationClip& clip, float time, Pose& pose)
    {
        for (const AnimationChannel& channel : clip.channels)
        {
            if (channel.node < 0 || channel.node >= static_cast<int>(pose.size()))
            {
                continue;
            }
            const glm::vec4 v = SampleChannel(channel, time);
            NodeTransform& node = pose[channel.node];
            switch (channel.path)
            {
            case AnimationPath::Translation: node.translation = glm::vec3{ v }; break;
            case AnimationPath::Rotation: node.rotation = glm::quat(v.w, v.x, v.y, v.z); break;
            case AnimationPath::Scale: node.scale = glm::vec3{ v }; break;
            }
        }
    }

    std::vector<glm::mat4> ComputeWorldMatrices(
        std::span<const int> parents, const Pose& pose)
    {
        // glTF doesn't promise parents come before children, so each node
        // is resolved on demand and remembered.
        std::vector<glm::mat4> world(pose.size());
        std::vector<char> done(pose.size(), 0);
        const auto resolve = [&](int node, const auto& self) -> const glm::mat4& {
            if (!done[node])
            {
                const glm::mat4 local = pose[node].ToMatrix();
                const int parent = parents[node];
                world[node] = parent >= 0 ? self(parent, self) * local : local;
                done[node] = 1;
            }
            return world[node];
        };
        for (std::size_t n = 0; n < pose.size(); ++n)
        {
            resolve(static_cast<int>(n), resolve);
        }
        return world;
    }

    void ComputePalette(const Skin& skin, std::span<const glm::mat4> world,
                        std::vector<glm::mat4>& palette)
    {
        palette.resize(skin.joints.size());
        for (std::size_t i = 0; i < skin.joints.size(); ++i)
        {
            palette[i] = world[skin.joints[i]] * skin.inverseBinds[i];
        }
    }

    void BlendPoses(const Pose& a, const Pose& b, float t, Pose& out)
    {
        out.resize(a.size());
        for (std::size_t n = 0; n < a.size(); ++n)
        {
            out[n].translation = glm::mix(a[n].translation, b[n].translation, t);
            out[n].scale = glm::mix(a[n].scale, b[n].scale, t);
            // glm::slerp already takes the short way (it flips b when the
            // dot product is negative).
            out[n].rotation = glm::normalize(glm::slerp(a[n].rotation, b[n].rotation, t));
        }
    }

    glm::vec3 SkinPoint(const glm::vec3& position, const SkinVertex& skin,
                        std::span<const glm::mat4> palette)
    {
        glm::mat4 blended{ 0.0f };
        for (int i = 0; i < 4; ++i)
        {
            if (skin.weights[i] > 0.0f)
            {
                blended += palette[skin.joints[i]] * skin.weights[i];
            }
        }
        return glm::vec3{ blended * glm::vec4{ position, 1.0f } };
    }
}
