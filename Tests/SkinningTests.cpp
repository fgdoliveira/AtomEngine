#include "Assets/Model.h"
#include "TestAssets.h" // v0.0.14: source assets in two roots
#include "Assets/Skin.h"

#include <doctest/doctest.h>

#include <glm/gtc/matrix_transform.hpp>

#include <cmath>
#include <string>

using namespace Atom;

namespace
{
    const std::string Rudy = AtomTests::Asset("ThirdParty/rudy.glb");

    bool Near(const glm::vec3& a, const glm::vec3& b, float tolerance = 1e-4f)
    {
        return glm::all(glm::lessThanEqual(glm::abs(a - b), glm::vec3{ tolerance }));
    }

    // Two joints standing on each other: the root at the origin, the
    // second one metre up (a forearm on an upper arm).
    Skeleton TwoJoints()
    {
        Skeleton skeleton;
        skeleton.parents = { -1, 0 };
        skeleton.rest.resize(2);
        skeleton.rest[1].translation = { 0.0f, 1.0f, 0.0f };
        Skin skin;
        skin.joints = { 0, 1 };
        // Bound where they stand: inverse of each joint's world at rest.
        skin.inverseBinds = { glm::mat4{ 1.0f },
                              glm::translate(glm::mat4{ 1.0f }, glm::vec3{ 0.0f, -1.0f, 0.0f }) };
        skeleton.skins.push_back(skin);
        return skeleton;
    }
}

TEST_CASE("Rudy's skin loads: 22 joints, every vertex weighted to 1")
{
    const Skeleton skeleton = LoadModelSkeleton(Rudy);
    REQUIRE(skeleton.skins.size() == 1);
    CHECK(skeleton.skins[0].joints.size() == 22);
    CHECK(skeleton.skins[0].joints.size() <= MaxSkinJoints);

    const std::vector<PrimitiveGeometry> geometry = LoadModelGeometry(Rudy);
    REQUIRE(!geometry.empty());
    for (const PrimitiveGeometry& primitive : geometry)
    {
        REQUIRE(primitive.skin.size() == primitive.vertices.size());
        for (const SkinVertex& skin : primitive.skin)
        {
            const float sum = skin.weights.x + skin.weights.y + skin.weights.z + skin.weights.w;
            CHECK(sum == doctest::Approx(1.0f).epsilon(1e-5));
            for (int j = 0; j < 4; ++j)
            {
                CHECK(skin.joints[j] < 22);
            }
        }
    }
}

TEST_CASE("At the bind pose, every palette matrix is the identity")
{
    // The rest pose is the pose the mesh was bound in, so joint world times
    // inverse bind cancels out and the mesh stands as modelled.
    const Skeleton skeleton = LoadModelSkeleton(Rudy);
    REQUIRE(skeleton.skins.size() == 1);
    const std::vector<glm::mat4> world = ComputeWorldMatrices(skeleton.parents, skeleton.rest);
    std::vector<glm::mat4> palette;
    ComputePalette(skeleton.skins[0], world, palette);
    REQUIRE(palette.size() == 22);
    for (const glm::mat4& m : palette)
    {
        for (int c = 0; c < 4; ++c)
        {
            for (int r = 0; r < 4; ++r)
            {
                CHECK(m[c][r] == doctest::Approx(c == r ? 1.0f : 0.0f).epsilon(1e-3));
            }
        }
    }
}

TEST_CASE("Skinning a vertex matches the hand computation")
{
    Skeleton skeleton = TwoJoints();
    // Bend the upper joint 90 degrees around Z.
    skeleton.rest[1].rotation = glm::angleAxis(glm::radians(90.0f), glm::vec3{ 0.0f, 0.0f, 1.0f });
    const std::vector<glm::mat4> world = ComputeWorldMatrices(skeleton.parents, skeleton.rest);
    std::vector<glm::mat4> palette;
    ComputePalette(skeleton.skins[0], world, palette);

    const glm::vec3 position{ 1.0f, 1.0f, 0.0f }; // one metre right of the elbow

    SUBCASE("fully on the bent joint: it swings round the elbow")
    {
        // Into the joint's space (0, 0, 0) + (1, 0, 0); rotated: (0, 1, 0);
        // back out at the elbow: (0, 2, 0).
        SkinVertex skin;
        skin.joints = { 1, 0, 0, 0 };
        skin.weights = { 1.0f, 0.0f, 0.0f, 0.0f };
        CHECK(Near(SkinPoint(position, skin, palette), glm::vec3{ 0.0f, 2.0f, 0.0f }));
    }
    SUBCASE("half and half: halfway between the two answers")
    {
        // The root doesn't move: (1, 1, 0); the bent joint gives (0, 2, 0).
        SkinVertex skin;
        skin.joints = { 0, 1, 0, 0 };
        skin.weights = { 0.5f, 0.5f, 0.0f, 0.0f };
        CHECK(Near(SkinPoint(position, skin, palette), glm::vec3{ 0.5f, 1.5f, 0.0f }));
    }
}

TEST_CASE("Clips move joints away from the rest pose")
{
    const Skeleton skeleton = LoadModelSkeleton(Rudy);
    const std::vector<AnimationClip> clips = LoadModelAnimations(Rudy);
    REQUIRE(clips.size() == 4);
    for (const AnimationClip& clip : clips)
    {
        Pose pose = skeleton.rest;
        ApplyClip(clip, clip.duration * 0.5f, pose);
        bool moved = false;
        for (std::size_t n = 0; n < pose.size(); ++n)
        {
            moved = moved || std::abs(glm::dot(pose[n].rotation, skeleton.rest[n].rotation)) < 0.999f;
        }
        CHECK_MESSAGE(moved, clip.name);
    }
}

TEST_CASE("Skinned bounds hold the model in every pose of every clip")
{
    // Culling tests the mesh box, so it must contain the character however
    // it moves, including between the times the box was sampled at.
    const Skeleton skeleton = LoadModelSkeleton(Rudy);
    const std::vector<AnimationClip> clips = LoadModelAnimations(Rudy);
    const std::vector<PrimitiveGeometry> geometry = LoadModelGeometry(Rudy);
    REQUIRE(!geometry.empty());
    const PrimitiveGeometry& body = geometry[0];

    glm::vec3 low{ 0.0f };
    glm::vec3 high{ 0.0f };
    ComputeSkinnedBounds(body, skeleton, 0, clips, 16, 0.05f, low, high);

    std::vector<glm::mat4> palette;
    int outside = 0;
    for (const AnimationClip& clip : clips)
    {
        for (int i = 0; i < 37; ++i) // off the sampling grid
        {
            Pose pose = skeleton.rest;
            ApplyClip(clip, clip.duration * (static_cast<float>(i) + 0.37f) / 37.0f, pose);
            ComputePalette(skeleton.skins[0], ComputeWorldMatrices(skeleton.parents, pose), palette);
            for (std::size_t v = 0; v < body.vertices.size(); v += 7)
            {
                const glm::vec3 p = SkinPoint(body.vertices[v].position, body.skin[v], palette);
                outside += glm::any(glm::lessThan(p, low)) || glm::any(glm::greaterThan(p, high));
            }
        }
    }
    CHECK(outside == 0);
}
