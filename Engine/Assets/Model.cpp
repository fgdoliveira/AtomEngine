#include "Assets/Model.h"

#include "Renderer/Renderer.h"

#include <cgltf.h>
#include <stb_image.h>

#include <glm/gtc/type_ptr.hpp>

#include <filesystem>
#include <iostream>
#include <unordered_map>

namespace Atom
{
    namespace
    {
        struct GltfDeleter
        {
            void operator()(cgltf_data* data) const { cgltf_free(data); }
        };

        struct StbDeleter
        {
            void operator()(stbi_uc* pixels) const { stbi_image_free(pixels); }
        };

        using StbPixels = std::unique_ptr<stbi_uc, StbDeleter>;

        StbPixels DecodeImage(
            const cgltf_image& image,
            const std::filesystem::path& baseDirectory,
            int& width,
            int& height
        )
        {
            int channels = 0;
            stbi_uc* pixels = nullptr;

            if (image.buffer_view)
            {
                const cgltf_buffer_view& view = *image.buffer_view;
                const auto* bytes =
                    static_cast<const stbi_uc*>(view.buffer->data) + view.offset;
                pixels = stbi_load_from_memory(
                    bytes,
                    static_cast<int>(view.size),
                    &width,
                    &height,
                    &channels,
                    4
                );
            }
            else if (image.uri)
            {
                const std::string path = (baseDirectory / image.uri).string();
                pixels = stbi_load(path.c_str(), &width, &height, &channels, 4);
            }

            if (!pixels)
            {
                std::cerr
                    << "Failed to decode glTF image '"
                    << (image.name ? image.name : "<unnamed>") << "': "
                    << stbi_failure_reason()
                    << '\n';
            }

            return StbPixels(pixels);
        }

        const cgltf_accessor* FindAttribute(
            const cgltf_primitive& primitive,
            cgltf_attribute_type type
        )
        {
            for (cgltf_size i = 0; i < primitive.attributes_count; ++i)
            {
                const cgltf_attribute& attribute = primitive.attributes[i];
                if (attribute.type == type && attribute.index == 0)
                {
                    return attribute.data;
                }
            }
            return nullptr;
        }
    }

    std::unique_ptr<Model> Model::Load(
        Renderer& renderer,
        const std::string& path
    )
    {
        cgltf_options options{};
        cgltf_data* rawData = nullptr;

        if (cgltf_parse_file(&options, path.c_str(), &rawData)
            != cgltf_result_success)
        {
            std::cerr << "Failed to parse glTF '" << path << "'.\n";
            return nullptr;
        }

        std::unique_ptr<cgltf_data, GltfDeleter> data(rawData);

        if (cgltf_load_buffers(&options, data.get(), path.c_str())
            != cgltf_result_success
            || cgltf_validate(data.get()) != cgltf_result_success)
        {
            std::cerr << "Failed to load/validate glTF '" << path << "'.\n";
            return nullptr;
        }

        const std::filesystem::path baseDirectory =
            std::filesystem::path(path).parent_path();

        auto model = std::make_unique<Model>();

        // Images -> textures (all glTF base color images are sRGB).
        std::unordered_map<const cgltf_image*, const Texture*> textures;
        for (cgltf_size i = 0; i < data->images_count; ++i)
        {
            const cgltf_image& image = data->images[i];
            int width = 0;
            int height = 0;
            const StbPixels pixels =
                DecodeImage(image, baseDirectory, width, height);
            if (!pixels)
            {
                continue;
            }

            auto texture = renderer.CreateTexture(
                static_cast<std::uint32_t>(width),
                static_cast<std::uint32_t>(height),
                pixels.get()
            );
            if (!texture)
            {
                return nullptr;
            }

            textures[&image] = texture.get();
            model->m_textures.push_back(std::move(texture));
        }

        // Materials; the extra trailing entry serves primitives without one.
        std::unordered_map<const cgltf_material*, std::size_t> materials;
        for (cgltf_size i = 0; i < data->materials_count; ++i)
        {
            const cgltf_material& source = data->materials[i];
            Material material{};

            if (source.has_pbr_metallic_roughness)
            {
                const cgltf_pbr_metallic_roughness& pbr =
                    source.pbr_metallic_roughness;
                material.baseColorFactor = glm::make_vec4(pbr.base_color_factor);

                const cgltf_texture* texture = pbr.base_color_texture.texture;
                if (texture && texture->image)
                {
                    const auto found = textures.find(texture->image);
                    if (found != textures.end())
                    {
                        material.baseColorTexture = found->second;
                    }
                }
            }

            float emissiveStrength = 1.0f;
            if (source.has_emissive_strength)
            {
                emissiveStrength = source.emissive_strength.emissive_strength;
            }
            material.emissiveFactor =
                glm::make_vec3(source.emissive_factor) * emissiveStrength;

            materials[&source] = model->m_materials.size();
            model->m_materials.push_back(material);
        }
        const std::size_t fallbackMaterial = model->m_materials.size();
        model->m_materials.push_back(Material{});

        // Meshes: one GPU mesh per triangle primitive.
        std::unordered_map<const cgltf_primitive*, const Mesh*> meshes;
        std::unordered_map<const cgltf_primitive*, std::size_t> primitiveMaterials;
        for (cgltf_size m = 0; m < data->meshes_count; ++m)
        {
            const cgltf_mesh& mesh = data->meshes[m];
            for (cgltf_size p = 0; p < mesh.primitives_count; ++p)
            {
                const cgltf_primitive& primitive = mesh.primitives[p];
                if (primitive.type != cgltf_primitive_type_triangles)
                {
                    continue;
                }

                const cgltf_accessor* positions =
                    FindAttribute(primitive, cgltf_attribute_type_position);
                const cgltf_accessor* normals =
                    FindAttribute(primitive, cgltf_attribute_type_normal);
                const cgltf_accessor* uvs =
                    FindAttribute(primitive, cgltf_attribute_type_texcoord);
                if (!positions)
                {
                    continue;
                }

                std::vector<Vertex> vertices(positions->count);
                for (cgltf_size v = 0; v < positions->count; ++v)
                {
                    Vertex& vertex = vertices[v];
                    cgltf_accessor_read_float(
                        positions, v, glm::value_ptr(vertex.position), 3);
                    vertex.normal = glm::vec3{ 0.0f, 1.0f, 0.0f };
                    if (normals)
                    {
                        cgltf_accessor_read_float(
                            normals, v, glm::value_ptr(vertex.normal), 3);
                    }
                    vertex.uv = glm::vec2{ 0.0f };
                    if (uvs)
                    {
                        cgltf_accessor_read_float(
                            uvs, v, glm::value_ptr(vertex.uv), 2);
                    }
                }

                std::vector<std::uint32_t> indices;
                if (primitive.indices)
                {
                    indices.resize(primitive.indices->count);
                    for (cgltf_size i = 0; i < indices.size(); ++i)
                    {
                        indices[i] = static_cast<std::uint32_t>(
                            cgltf_accessor_read_index(primitive.indices, i));
                    }
                }
                else
                {
                    indices.resize(vertices.size());
                    for (std::uint32_t i = 0; i < indices.size(); ++i)
                    {
                        indices[i] = i;
                    }
                }

                auto gpuMesh = renderer.CreateMesh(vertices, indices);
                if (!gpuMesh)
                {
                    return nullptr;
                }

                meshes[&primitive] = gpuMesh.get();
                primitiveMaterials[&primitive] = primitive.material
                    ? materials.at(primitive.material)
                    : fallbackMaterial;
                model->m_meshes.push_back(std::move(gpuMesh));
            }
        }

        // Nodes -> parts, with baked world transforms.
        for (cgltf_size n = 0; n < data->nodes_count; ++n)
        {
            const cgltf_node& node = data->nodes[n];
            if (!node.mesh)
            {
                continue;
            }

            glm::mat4 world{ 1.0f };
            cgltf_node_transform_world(&node, glm::value_ptr(world));

            for (cgltf_size p = 0; p < node.mesh->primitives_count; ++p)
            {
                const cgltf_primitive* primitive = &node.mesh->primitives[p];
                const auto found = meshes.find(primitive);
                if (found == meshes.end())
                {
                    continue;
                }

                model->m_parts.push_back(Part{
                    found->second,
                    primitiveMaterials.at(primitive),
                    world
                });
            }
        }

        std::cout
            << "Loaded model '" << path << "': "
            << model->m_parts.size() << " parts, "
            << model->m_textures.size() << " textures\n";

        return model;
    }

    void Model::Submit(Renderer& renderer, const glm::mat4& transform) const
    {
        for (const Part& part : m_parts)
        {
            renderer.Submit(
                *part.mesh,
                m_materials[part.materialIndex],
                transform * part.transform
            );
        }
    }
}
