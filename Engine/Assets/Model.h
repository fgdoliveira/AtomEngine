#pragma once

#include "Renderer/Material.h"
#include "Renderer/Mesh.h"
#include "Renderer/Texture.h"

#include <glm/mat4x4.hpp>

#include <memory>
#include <string>
#include <vector>

namespace Atom
{
    class Renderer;

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

        // Loads a .glb/.gltf (triangles with POSITION, NORMAL, TEXCOORD_0;
        // base color textures and factors; emissive factor).
        static std::unique_ptr<Model> Load(
            Renderer& renderer,
            const std::string& path
        );

        // Queues every part, placed by `transform`.
        void Submit(Renderer& renderer, const glm::mat4& transform) const;

        const std::vector<Part>& GetParts() const { return m_parts; }

    private:
        std::vector<std::unique_ptr<Texture>> m_textures;
        std::vector<std::unique_ptr<Mesh>> m_meshes;
        std::vector<Material> m_materials; // last entry is the fallback
        std::vector<Part> m_parts;
    };
}
