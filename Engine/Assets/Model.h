#pragma once

#include "Renderer/Material.h"
#include "Renderer/Mesh.h"
#include "Renderer/Texture.h"

#include <glm/mat4x4.hpp>

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
    };

    // Every triangle primitive of a glTF file, in file order. For tools and
    // tests; empty if the file can't be loaded.
    std::vector<PrimitiveGeometry> LoadModelGeometry(const std::string& path);

    // A glTF scene flattened into drawable parts. Owns its GPU resources.
    class Model
    {
    public:
        struct Part
        {
            const Mesh* mesh = nullptr;
            std::size_t materialIndex = 0;
            glm::mat4 transform{ 1.0f }; // node world transform
        };

        // Loads a .glb/.gltf (triangles with POSITION, NORMAL, TEXCOORD_0,
        // optional COLOR_0 as baked light; base color textures and factors;
        // emissive factor).
        static std::unique_ptr<Model> Load(
            Renderer& renderer,
            const std::string& path
        );

        // Queues every part, placed by `transform`.
        void Submit(Renderer& renderer, const glm::mat4& transform) const;

        const std::vector<Part>& GetParts() const { return m_parts; }

        // Materials can be tweaked at runtime (e.g. flickering emission);
        // every part using the material follows. nullptr if not found.
        Material* FindMaterial(std::string_view name);

    private:
        std::vector<std::unique_ptr<Texture>> m_textures;
        std::vector<std::unique_ptr<Mesh>> m_meshes;
        std::vector<Material> m_materials; // last entry is the fallback
        std::vector<std::string> m_materialNames; // parallel to m_materials
        std::vector<Part> m_parts;
    };
}
