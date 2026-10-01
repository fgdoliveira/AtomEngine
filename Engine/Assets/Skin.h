#pragma once

#include "Assets/Animation.h"
#include "Renderer/Mesh.h" // SkinVertex

#include <glm/gtc/quaternion.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <array>
#include <cstdint>
#include <span>
#include <vector>

namespace Atom
{
    // Skeletal skinning (M35). A skeleton is a hierarchy of joints (glTF
    // nodes); a skinned vertex follows up to four of them, blended by
    // weight. Each joint has an inverse bind matrix: it takes a vertex from
    // model space into the joint's own space as it stood when the mesh was
    // bound to it. Posing the joint and multiplying back out moves the
    // vertex with it:
    //
    //     skinned = sum_i weight_i * (jointWorld_i * inverseBind_i) * position
    //
    // The bracket is the joint's palette matrix, computed once per joint on
    // the CPU and sent to the vertex shader; at the bind pose it is the
    // identity and the mesh stands as modelled.

    // The most joints one skinned draw may use (the palette is a uniform).
    inline constexpr std::size_t MaxSkinJoints = 64;

    // A node's local transform, T * R * S.
    struct NodeTransform
    {
        glm::vec3 translation{ 0.0f };
        glm::quat rotation{ 1.0f, 0.0f, 0.0f, 0.0f };
        glm::vec3 scale{ 1.0f };

        glm::mat4 ToMatrix() const;
    };

    // Local transforms for every node of a model, in node order.
    using Pose = std::vector<NodeTransform>;

    struct Skin
    {
        std::vector<int> joints;               // node index of each joint
        std::vector<glm::mat4> inverseBinds;   // parallel to joints
    };

    // The node hierarchy of a model with its rest pose, and its skins.
    struct Skeleton
    {
        std::vector<int> parents; // -1 for roots
        Pose rest;
        std::vector<Skin> skins;
    };

    // Overwrites the nodes `clip` animates with its values at `time`.
    void ApplyClip(const AnimationClip& clip, float time, Pose& pose);

    // Model-space matrix of every node: its local transform under its
    // parent's, all the way up.
    std::vector<glm::mat4> ComputeWorldMatrices(
        std::span<const int> parents, const Pose& pose);

    // palette[i] = world(joint i) * inverseBind(i).
    void ComputePalette(const Skin& skin, std::span<const glm::mat4> world,
                        std::vector<glm::mat4>& palette);

    // Pose blending (M37): per node, translation and scale lerped, rotation
    // slerped the short way round (q and -q are the same rotation; picking
    // the nearer keeps a blend from spinning the long way). t = 0 gives a.
    void BlendPoses(const Pose& a, const Pose& b, float t, Pose& out);

    // One clip's contribution to a blended pose.
    struct ClipSample
    {
        int clip = -1;
        float time = 0.0f;
        float weight = 1.0f;
    };

    // The shader's sum, on the CPU: reference for tests and bounds.
    glm::vec3 SkinPoint(const glm::vec3& position, const SkinVertex& skin,
                        std::span<const glm::mat4> palette);
}
