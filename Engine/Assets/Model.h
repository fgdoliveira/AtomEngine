#pragma once

#include "Assets/Animation.h"
#include "Assets/Skin.h"
#include "Renderer/Material.h"
#include "Renderer/Mesh.h"
#include "Renderer/Texture.h"

#include <glm/gtc/quaternion.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace Atom
{
    class Renderer;

    // The CPU half of loading one triangle primitive: no GPU involved.
    struct PrimitiveGeometry
    {
        std::vector<Vertex> vertices;
        std::vector<std::uint32_t> indices;
        bool hasBakedLight = false; // had COLOR_0
        bool hasLightmapUv = false; // had TEXCOORD_1
        // JOINTS_0 / WEIGHTS_0 (M35): one per vertex when skinned, weights
        // renormalised to sum to 1; empty otherwise.
        std::vector<SkinVertex> skin;
    };

    // How a material is drawn, without its GPU textures (tools and tests).
    struct MaterialInfo
    {
        std::string name;
        AlphaMode alphaMode = AlphaMode::Opaque;
        float alphaCutoff = 0.5f;
        bool doubleSided = false;
        bool hasEmissiveTexture = false;
        float fogAmount = 1.0f;
        float wet = 0.0f;
    };

    // Every material of a glTF file, in file order; empty on failure.
    std::vector<MaterialInfo> LoadModelMaterials(const std::string& path);

    // Every animation clip of a glTF file (tools and tests).
    std::vector<AnimationClip> LoadModelAnimations(const std::string& path);

    // Every triangle primitive of a glTF file, in file order. For tools and
    // tests; empty if the file can't be loaded.
    std::vector<PrimitiveGeometry> LoadModelGeometry(const std::string& path);

    // The node hierarchy, rest pose and skins of a glTF file (tools and
    // tests); empty on failure.
    Skeleton LoadModelSkeleton(const std::string& path);

    // Box around the skinned `geometry` in every pose of `clips` (sampled
    // `samplesPerClip` times each) and the rest pose, grown by `margin` of
    // its size for the motion between samples.
    void ComputeSkinnedBounds(const PrimitiveGeometry& geometry, const Skeleton& skeleton,
                              int skin, std::span<const AnimationClip> clips,
                              int samplesPerClip, float margin,
                              glm::vec3& low, glm::vec3& high);

    // A glTF scene flattened into drawable parts. Owns its GPU resources.
    class Model
    {
    public:
        struct Part
        {
            const Mesh* mesh = nullptr;
            std::size_t materialIndex = 0;
            glm::mat4 transform{ 1.0f }; // node world transform (rest pose)
            int node = -1; // set when an animation can move it
            int skin = -1; // skinned (M35): drawn by its skin's joints
        };

        // Loads a .glb/.gltf (triangles with POSITION, NORMAL, TEXCOORD_0,
        // optional COLOR_0 as baked light; base color textures and factors;
        // emissive factor; skins of up to 64 joints).
        static std::unique_ptr<Model> Load(
            Renderer& renderer,
            const std::string& path
        );

        // Queues every part, placed by `transform`. With a clip, animated
        // parts are posed at `time` (seconds into the clip); the rest keep
        // their baked transforms.
        void Submit(Renderer& renderer, const glm::mat4& transform,
                    int clip = -1, float time = 0.0f) const;

        // Queues every part in an explicit pose (one local transform per
        // node, e.g. from SamplePose or a blend of two). Skinned parts
        // follow their joints; the rest follow their node.
        void Submit(Renderer& renderer, const glm::mat4& transform, const Pose& pose) const;

        // The rest pose with `clip` (if any) applied at `time`.
        void SamplePose(int clip, float time, Pose& pose) const;
        const Pose& GetRestPose() const { return m_skeleton.rest; }
        const Skeleton& GetSkeleton() const { return m_skeleton; }
        bool IsSkinned() const { return !m_skeleton.skins.empty(); }

        // World-space box around every part in its rest pose.
        const glm::vec3& GetBoundsMin() const { return m_boundsMin; }
        const glm::vec3& GetBoundsMax() const { return m_boundsMax; }

        // -1 if the model has no clip of that name.
        int FindClip(std::string_view name) const;
        const AnimationClip* GetClip(int clip) const;

        const std::vector<Part>& GetParts() const { return m_parts; }

        // Materials can be tweaked at runtime (e.g. flickering emission);
        // every part using the material follows. nullptr if not found.
        Material* FindMaterial(std::string_view name);

        // Every part is lit by this baked texture (its TEXCOORD_1) instead
        // of the ambient term. The texture must outlive the model's use.
        void SetLightmap(const Texture* lightmap, float intensity);

    private:
        std::vector<std::unique_ptr<Texture>> m_textures;
        std::vector<std::unique_ptr<Mesh>> m_meshes;
        std::vector<Material> m_materials; // last entry is the fallback
        std::vector<std::string> m_materialNames; // parallel to m_materials
        std::vector<Part> m_parts;
        Skeleton m_skeleton;
        bool m_animated = false; // some part moves with a clip or a skin
        std::vector<AnimationClip> m_clips;
        glm::vec3 m_boundsMin{ 0.0f };
        glm::vec3 m_boundsMax{ 0.0f };
    };
}
