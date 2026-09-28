#include "Assets/Model.h"

#include "Renderer/Renderer.h"

#include <cgltf.h>
#include <stb_image.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <optional>
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
            cgltf_attribute_type type,
            cgltf_int index = 0
        )
        {
            for (cgltf_size i = 0; i < primitive.attributes_count; ++i)
            {
                const cgltf_attribute& attribute = primitive.attributes[i];
                if (attribute.type == type && attribute.index == index)
                {
                    return attribute.data;
                }
            }
            return nullptr;
        }

        using GltfData = std::unique_ptr<cgltf_data, GltfDeleter>;

        GltfData ParseGltf(const std::string& path)
        {
            cgltf_options options{};
            cgltf_data* rawData = nullptr;
            if (cgltf_parse_file(&options, path.c_str(), &rawData)
                != cgltf_result_success)
            {
                std::cerr << "Failed to parse glTF '" << path << "'.\n";
                return nullptr;
            }

            GltfData data(rawData);
            if (cgltf_load_buffers(&options, data.get(), path.c_str())
                != cgltf_result_success
                || cgltf_validate(data.get()) != cgltf_result_success)
            {
                std::cerr << "Failed to load/validate glTF '" << path << "'.\n";
                return nullptr;
            }
            return data;
        }

        std::uint16_t ToUnorm16(float value)
        {
            return static_cast<std::uint16_t>(std::lround(std::clamp(value, 0.0f, 1.0f) * 65535.0f));
        }

        // glTF alpha: OPAQUE, MASK (with a cutoff) or BLEND (decals).
        void ReadAlpha(const cgltf_material& source, Material& material)
        {
            // AtomEngine's own settings travel in the material's glTF extras
            // (Blender custom properties): {"atom_fog": 0.5}.
            if (source.extras.data)
            {
                if (const char* key = std::strstr(source.extras.data, "\"atom_fog\""))
                {
                    if (const char* colon = std::strchr(key, ':'))
                    {
                        material.fogAmount = std::clamp(std::strtof(colon + 1, nullptr), 0.0f, 1.0f);
                    }
                }
            }
            material.doubleSided = source.double_sided != 0;
            if (source.alpha_mode == cgltf_alpha_mode_mask)
            {
                material.alphaMode = AlphaMode::Mask;
                material.alphaCutoff = source.alpha_cutoff;
            }
            else if (source.alpha_mode == cgltf_alpha_mode_blend)
            {
                material.alphaMode = AlphaMode::Blend;
            }
        }

        // nullopt for primitives that aren't drawable triangles.
        std::optional<PrimitiveGeometry> ReadPrimitive(const cgltf_primitive& primitive)
        {
            if (primitive.type != cgltf_primitive_type_triangles)
            {
                return std::nullopt;
            }

            const cgltf_accessor* positions =
                FindAttribute(primitive, cgltf_attribute_type_position);
            const cgltf_accessor* normals =
                FindAttribute(primitive, cgltf_attribute_type_normal);
            const cgltf_accessor* uvs =
                FindAttribute(primitive, cgltf_attribute_type_texcoord);
            const cgltf_accessor* lightmapUvs =
                FindAttribute(primitive, cgltf_attribute_type_texcoord, 1);
            const cgltf_accessor* colors =
                FindAttribute(primitive, cgltf_attribute_type_color);
            if (!positions)
            {
                return std::nullopt;
            }

            PrimitiveGeometry geometry;
            geometry.hasBakedLight = colors != nullptr;
            geometry.hasLightmapUv = lightmapUvs != nullptr;
            geometry.vertices.resize(positions->count);
            for (cgltf_size v = 0; v < positions->count; ++v)
            {
                Vertex& vertex = geometry.vertices[v];
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
                if (lightmapUvs)
                {
                    cgltf_accessor_read_float(
                        lightmapUvs, v, glm::value_ptr(vertex.lightmapUv), 2);
                }
                if (colors)
                {
                    // COLOR_0 may be RGB or RGBA, float or normalised ints;
                    // cgltf converts. Missing alpha stays 1.
                    glm::vec4 color{ 1.0f };
                    cgltf_accessor_read_float(
                        colors, v, glm::value_ptr(color), cgltf_num_components(colors->type));
                    for (int c = 0; c < 4; ++c)
                    {
                        vertex.color[c] = ToUnorm16(color[c]);
                    }
                }
            }

            if (primitive.indices)
            {
                geometry.indices.resize(primitive.indices->count);
                for (cgltf_size i = 0; i < geometry.indices.size(); ++i)
                {
                    geometry.indices[i] = static_cast<std::uint32_t>(
                        cgltf_accessor_read_index(primitive.indices, i));
                }
            }
            else
            {
                geometry.indices.resize(geometry.vertices.size());
                for (std::uint32_t i = 0; i < geometry.indices.size(); ++i)
                {
                    geometry.indices[i] = i;
                }
            }
            return geometry;
        }
    }

    namespace
    {
        std::vector<AnimationClip> ReadAnimations(const cgltf_data& data)
        {
            std::vector<AnimationClip> clips;
            for (cgltf_size a = 0; a < data.animations_count; ++a)
            {
                const cgltf_animation& source = data.animations[a];
                AnimationClip clip;
                clip.name = source.name ? source.name : "";
                for (cgltf_size c = 0; c < source.channels_count; ++c)
                {
                    const cgltf_animation_channel& from = source.channels[c];
                    if (!from.target_node || !from.sampler)
                    {
                        continue;
                    }
                    AnimationChannel channel;
                    channel.node = static_cast<int>(from.target_node - data.nodes);
                    switch (from.target_path)
                    {
                    case cgltf_animation_path_type_translation:
                        channel.path = AnimationPath::Translation;
                        break;
                    case cgltf_animation_path_type_rotation:
                        channel.path = AnimationPath::Rotation;
                        break;
                    case cgltf_animation_path_type_scale:
                        channel.path = AnimationPath::Scale;
                        break;
                    default:
                        continue; // morph weights: not supported
                    }
                    switch (from.sampler->interpolation)
                    {
                    case cgltf_interpolation_type_step:
                        channel.interpolation = Interpolation::Step;
                        break;
                    case cgltf_interpolation_type_cubic_spline:
                        channel.interpolation = Interpolation::CubicSpline;
                        break;
                    default:
                        channel.interpolation = Interpolation::Linear;
                        break;
                    }

                    const cgltf_accessor* input = from.sampler->input;
                    const cgltf_accessor* output = from.sampler->output;
                    channel.times.resize(input->count);
                    for (cgltf_size k = 0; k < input->count; ++k)
                    {
                        cgltf_accessor_read_float(input, k, &channel.times[k], 1);
                    }
                    const cgltf_size components = cgltf_num_components(output->type);
                    channel.values.resize(output->count, glm::vec4{ 0.0f });
                    for (cgltf_size k = 0; k < output->count; ++k)
                    {
                        cgltf_accessor_read_float(
                            output, k, glm::value_ptr(channel.values[k]), components);
                    }
                    if (!channel.times.empty())
                    {
                        clip.duration = std::max(clip.duration, channel.times.back());
                    }
                    clip.channels.push_back(std::move(channel));
                }
                clips.push_back(std::move(clip));
            }
            return clips;
        }
    }

    std::vector<AnimationClip> LoadModelAnimations(const std::string& path)
    {
        const GltfData data = ParseGltf(path);
        return data ? ReadAnimations(*data) : std::vector<AnimationClip>{};
    }

    std::vector<MaterialInfo> LoadModelMaterials(const std::string& path)
    {
        std::vector<MaterialInfo> result;
        const GltfData data = ParseGltf(path);
        if (!data)
        {
            return result;
        }
        for (cgltf_size i = 0; i < data->materials_count; ++i)
        {
            const cgltf_material& source = data->materials[i];
            Material material{};
            ReadAlpha(source, material);
            result.push_back(MaterialInfo{
                source.name ? source.name : "",
                material.alphaMode,
                material.alphaCutoff,
                material.doubleSided,
                source.emissive_texture.texture != nullptr,
                material.fogAmount });
        }
        return result;
    }

    std::vector<PrimitiveGeometry> LoadModelGeometry(const std::string& path)
    {
        std::vector<PrimitiveGeometry> result;
        const GltfData data = ParseGltf(path);
        if (!data)
        {
            return result;
        }
        for (cgltf_size m = 0; m < data->meshes_count; ++m)
        {
            for (cgltf_size p = 0; p < data->meshes[m].primitives_count; ++p)
            {
                if (auto geometry = ReadPrimitive(data->meshes[m].primitives[p]))
                {
                    result.push_back(std::move(*geometry));
                }
            }
        }
        return result;
    }

    std::unique_ptr<Model> Model::Load(
        Renderer& renderer,
        const std::string& path
    )
    {
        const GltfData data = ParseGltf(path);
        if (!data)
        {
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
            // An emissive texture that is just the base colour again (older
            // kit pieces glow by their base colour) is left out: same look,
            // one binding fewer.
            if (const cgltf_texture* emissive = source.emissive_texture.texture;
                emissive && emissive->image
                && !(source.has_pbr_metallic_roughness
                     && source.pbr_metallic_roughness.base_color_texture.texture
                     && source.pbr_metallic_roughness.base_color_texture.texture->image == emissive->image))
            {
                if (const auto found = textures.find(emissive->image); found != textures.end())
                {
                    material.emissiveTexture = found->second;
                }
            }
            ReadAlpha(source, material);

            materials[&source] = model->m_materials.size();
            model->m_materials.push_back(material);
            model->m_materialNames.emplace_back(source.name ? source.name : "");
        }
        const std::size_t fallbackMaterial = model->m_materials.size();
        model->m_materials.push_back(Material{});
        model->m_materialNames.emplace_back();

        // Meshes: one GPU mesh per triangle primitive.
        std::unordered_map<const cgltf_primitive*, const Mesh*> meshes;
        std::unordered_map<const cgltf_primitive*, std::size_t> primitiveMaterials;
        for (cgltf_size m = 0; m < data->meshes_count; ++m)
        {
            const cgltf_mesh& mesh = data->meshes[m];
            for (cgltf_size p = 0; p < mesh.primitives_count; ++p)
            {
                const cgltf_primitive& primitive = mesh.primitives[p];
                const std::optional<PrimitiveGeometry> geometry = ReadPrimitive(primitive);
                if (!geometry)
                {
                    continue;
                }

                auto gpuMesh = renderer.CreateMesh(
                    geometry->vertices, geometry->indices, geometry->hasBakedLight);
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

        // Node hierarchy and clips. A node moves if a clip targets it or
        // any of its ancestors.
        model->m_clips = ReadAnimations(*data);
        model->m_nodes.resize(data->nodes_count);
        for (cgltf_size n = 0; n < data->nodes_count; ++n)
        {
            const cgltf_node& source = data->nodes[n];
            Node& node = model->m_nodes[n];
            node.parent = source.parent ? static_cast<int>(source.parent - data->nodes) : -1;
            if (source.has_translation)
            {
                node.translation = glm::make_vec3(source.translation);
            }
            if (source.has_rotation)
            {
                const float* r = source.rotation; // x y z w
                node.rotation = glm::quat(r[3], r[0], r[1], r[2]);
            }
            if (source.has_scale)
            {
                node.scale = glm::make_vec3(source.scale);
            }
        }
        std::vector<bool> targeted(data->nodes_count, false);
        for (const AnimationClip& clip : model->m_clips)
        {
            for (const AnimationChannel& channel : clip.channels)
            {
                targeted[channel.node] = true;
            }
        }
        const auto moves = [&](int node) {
            for (; node >= 0; node = model->m_nodes[node].parent)
            {
                if (targeted[node])
                {
                    return true;
                }
            }
            return false;
        };

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
            const int animatedNode = moves(static_cast<int>(n)) ? static_cast<int>(n) : -1;

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
                    world,
                    animatedNode
                });
            }
        }

        // Bounds of the whole model: every part's mesh box, transformed.
        bool first = true;
        for (const Part& part : model->m_parts)
        {
            const glm::vec3 low = part.mesh->GetBoundsMin();
            const glm::vec3 high = part.mesh->GetBoundsMax();
            for (int corner = 0; corner < 8; ++corner)
            {
                const glm::vec3 local{
                    (corner & 1) ? high.x : low.x,
                    (corner & 2) ? high.y : low.y,
                    (corner & 4) ? high.z : low.z };
                const glm::vec3 world{ part.transform * glm::vec4{ local, 1.0f } };
                model->m_boundsMin = first ? world : glm::min(model->m_boundsMin, world);
                model->m_boundsMax = first ? world : glm::max(model->m_boundsMax, world);
                first = false;
            }
        }

        std::cout
            << "Loaded model '" << path << "': "
            << model->m_parts.size() << " parts, "
            << model->m_textures.size() << " textures\n";

        return model;
    }

    void Model::SetLightmap(const Texture* lightmap, float intensity)
    {
        for (Material& material : m_materials)
        {
            material.lightmap = lightmap;
            material.lightmapIntensity = intensity;
        }
    }

    Material* Model::FindMaterial(std::string_view name)
    {
        for (std::size_t i = 0; i < m_materialNames.size(); ++i)
        {
            if (m_materialNames[i] == name)
            {
                return &m_materials[i];
            }
        }
        return nullptr;
    }

    int Model::FindClip(std::string_view name) const
    {
        for (std::size_t i = 0; i < m_clips.size(); ++i)
        {
            if (m_clips[i].name == name)
            {
                return static_cast<int>(i);
            }
        }
        return -1;
    }

    const AnimationClip* Model::GetClip(int clip) const
    {
        return clip >= 0 && clip < static_cast<int>(m_clips.size()) ? &m_clips[clip] : nullptr;
    }

    void Model::Submit(Renderer& renderer, const glm::mat4& transform, int clip, float time) const
    {
        const AnimationClip* playing = GetClip(clip);

        // Pose: rest transforms with the clip's channels applied.
        std::vector<Node> pose;
        std::vector<glm::mat4> world;
        std::vector<bool> done;
        if (playing)
        {
            pose = m_nodes;
            for (const AnimationChannel& channel : playing->channels)
            {
                const glm::vec4 v = SampleChannel(channel, time);
                Node& node = pose[channel.node];
                switch (channel.path)
                {
                case AnimationPath::Translation: node.translation = glm::vec3{ v }; break;
                case AnimationPath::Rotation: node.rotation = glm::quat(v.w, v.x, v.y, v.z); break;
                case AnimationPath::Scale: node.scale = glm::vec3{ v }; break;
                }
            }
            world.resize(pose.size());
            done.resize(pose.size(), false);
        }

        // World matrix of a posed node: its local T*R*S under its parent's.
        const auto nodeWorld = [&](int node, const auto& self) -> const glm::mat4& {
            if (!done[node])
            {
                const Node& n = pose[node];
                const glm::mat4 local = glm::translate(glm::mat4{ 1.0f }, n.translation)
                    * glm::mat4_cast(n.rotation) * glm::scale(glm::mat4{ 1.0f }, n.scale);
                world[node] = n.parent >= 0 ? self(n.parent, self) * local : local;
                done[node] = true;
            }
            return world[node];
        };

        for (const Part& part : m_parts)
        {
            const glm::mat4 partTransform = playing && part.node >= 0
                ? nodeWorld(part.node, nodeWorld)
                : part.transform;
            renderer.Submit(
                *part.mesh,
                m_materials[part.materialIndex],
                transform * partTransform
            );
        }
    }
}
