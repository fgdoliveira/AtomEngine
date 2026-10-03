#include "Assets/Model.h"
#include "Core/AssetLog.h"

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
#include <limits>
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
                AssetLog::Opened(path);
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
            AssetLog::Opened(path);
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
                const auto read = [&](const char* name, float& value) {
                    if (const char* key = std::strstr(source.extras.data, name))
                    {
                        if (const char* colon = std::strchr(key, ':'))
                        {
                            value = std::clamp(std::strtof(colon + 1, nullptr), 0.0f, 1.0f);
                        }
                    }
                };
                read("\"atom_fog\"", material.fogAmount);
                read("\"atom_wet\"", material.wet);
                read("\"atom_specular\"", material.specular);
                read("\"atom_reveal\"", material.reveal);
                read("\"atom_water\"", material.water);
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
            if (material.water > 0.0f)
            {
                material.alphaMode = AlphaMode::Blend; // drawn with the blended draws
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
            const cgltf_accessor* joints =
                FindAttribute(primitive, cgltf_attribute_type_joints);
            const cgltf_accessor* weights =
                FindAttribute(primitive, cgltf_attribute_type_weights);
            if (!positions)
            {
                return std::nullopt;
            }

            PrimitiveGeometry geometry;
            geometry.hasBakedLight = colors != nullptr;
            geometry.hasLightmapUv = lightmapUvs != nullptr;
            geometry.vertices.resize(positions->count);
            if (joints && weights)
            {
                geometry.skin.resize(positions->count);
            }
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
                if (!geometry.skin.empty())
                {
                    // Exporters store weights as floats or normalised ints,
                    // and their sum drifts from 1: renormalise, or the
                    // vertex would shrink toward the model's origin.
                    SkinVertex& skin = geometry.skin[v];
                    cgltf_uint indices[4]{};
                    cgltf_accessor_read_uint(joints, v, indices, 4);
                    cgltf_accessor_read_float(weights, v, glm::value_ptr(skin.weights), 4);
                    for (int j = 0; j < 4; ++j)
                    {
                        skin.joints[j] = static_cast<std::uint8_t>(std::min<cgltf_uint>(indices[j], 255));
                        skin.weights[j] = std::max(skin.weights[j], 0.0f);
                    }
                    const float sum = skin.weights.x + skin.weights.y + skin.weights.z + skin.weights.w;
                    skin.weights = sum > 0.0f ? skin.weights / sum : glm::vec4{ 1.0f, 0.0f, 0.0f, 0.0f };
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

    namespace
    {
        Skeleton ReadSkeleton(const cgltf_data& data)
        {
            Skeleton skeleton;
            skeleton.parents.resize(data.nodes_count, -1);
            skeleton.names.resize(data.nodes_count);
            skeleton.rest.resize(data.nodes_count);
            for (cgltf_size n = 0; n < data.nodes_count; ++n)
            {
                const cgltf_node& source = data.nodes[n];
                NodeTransform& node = skeleton.rest[n];
                skeleton.parents[n] = source.parent ? static_cast<int>(source.parent - data.nodes) : -1;
                skeleton.names[n] = source.name ? source.name : "";
                if (source.has_matrix)
                {
                    // Rare in exports; decompose so the node can be posed.
                    const glm::mat4 m = glm::make_mat4(source.matrix);
                    node.translation = glm::vec3{ m[3] };
                    node.scale = { glm::length(glm::vec3{ m[0] }), glm::length(glm::vec3{ m[1] }),
                                   glm::length(glm::vec3{ m[2] }) };
                    node.rotation = glm::quat_cast(glm::mat3{
                        glm::vec3{ m[0] } / node.scale.x, glm::vec3{ m[1] } / node.scale.y,
                        glm::vec3{ m[2] } / node.scale.z });
                }
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
            for (cgltf_size s = 0; s < data.skins_count; ++s)
            {
                const cgltf_skin& source = data.skins[s];
                Skin skin;
                skin.joints.resize(source.joints_count);
                skin.inverseBinds.resize(source.joints_count, glm::mat4{ 1.0f });
                for (cgltf_size j = 0; j < source.joints_count; ++j)
                {
                    skin.joints[j] = static_cast<int>(source.joints[j] - data.nodes);
                    if (source.inverse_bind_matrices)
                    {
                        cgltf_accessor_read_float(source.inverse_bind_matrices, j,
                                                  glm::value_ptr(skin.inverseBinds[j]), 16);
                    }
                }
                skeleton.skins.push_back(std::move(skin));
            }
            return skeleton;
        }
    }

    Skeleton LoadModelSkeleton(const std::string& path)
    {
        const GltfData data = ParseGltf(path);
        return data ? ReadSkeleton(*data) : Skeleton{};
    }

    void ComputeSkinnedBounds(const PrimitiveGeometry& geometry, const Skeleton& skeleton,
                              int skin, std::span<const AnimationClip> clips,
                              int samplesPerClip, float margin,
                              glm::vec3& low, glm::vec3& high)
    {
        low = glm::vec3{ std::numeric_limits<float>::max() };
        high = glm::vec3{ std::numeric_limits<float>::lowest() };
        std::vector<glm::mat4> palette;
        const auto cover = [&](const Pose& pose) {
            const std::vector<glm::mat4> world = ComputeWorldMatrices(skeleton.parents, pose);
            ComputePalette(skeleton.skins[skin], world, palette);
            for (std::size_t v = 0; v < geometry.vertices.size(); ++v)
            {
                const glm::vec3 p = SkinPoint(geometry.vertices[v].position, geometry.skin[v], palette);
                low = glm::min(low, p);
                high = glm::max(high, p);
            }
        };
        cover(skeleton.rest);
        for (const AnimationClip& clip : clips)
        {
            for (int i = 0; i <= samplesPerClip; ++i)
            {
                Pose pose = skeleton.rest;
                ApplyClip(clip, clip.duration * static_cast<float>(i) / static_cast<float>(samplesPerClip), pose);
                cover(pose);
            }
        }
        const glm::vec3 grow = (high - low) * margin;
        low -= grow;
        high += grow;
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
                material.fogAmount,
                material.wet });
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
            if (source.has_pbr_metallic_roughness)
            {
                material.roughness = std::clamp(source.pbr_metallic_roughness.roughness_factor, 0.0f, 1.0f);
            }

            materials[&source] = model->m_materials.size();
            model->m_materials.push_back(material);
            model->m_materialNames.emplace_back(source.name ? source.name : "");
        }
        const std::size_t fallbackMaterial = model->m_materials.size();
        model->m_materials.push_back(Material{});
        model->m_materialNames.emplace_back();

        model->m_clips = ReadAnimations(*data);
        model->m_skeleton = ReadSkeleton(*data);
        for (const Skin& skin : model->m_skeleton.skins)
        {
            if (skin.joints.size() > MaxSkinJoints)
            {
                std::cerr << "Model '" << path << "': a skin has " << skin.joints.size()
                          << " joints; at most " << MaxSkinJoints << " are supported.\n";
                return nullptr;
            }
        }

        // The skin each primitive is drawn with: that of the first node
        // using its mesh (glTF puts the skin on the node, not the mesh).
        std::unordered_map<const cgltf_mesh*, int> meshSkins;
        for (cgltf_size n = 0; n < data->nodes_count; ++n)
        {
            const cgltf_node& node = data->nodes[n];
            if (node.mesh && node.skin && !meshSkins.contains(node.mesh))
            {
                meshSkins[node.mesh] = static_cast<int>(node.skin - data->skins);
            }
        }

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

                const auto skinOf = meshSkins.find(&mesh);
                const bool skinned = skinOf != meshSkins.end() && !geometry->skin.empty();
                auto gpuMesh = renderer.CreateMesh(
                    geometry->vertices, geometry->indices, geometry->hasBakedLight,
                    skinned ? std::span<const SkinVertex>(geometry->skin) : std::span<const SkinVertex>{});
                if (!gpuMesh)
                {
                    return nullptr;
                }
                if (skinned)
                {
                    // Culling needs a box that holds every pose, not just
                    // the bind pose: skin it through each clip.
                    glm::vec3 low{ 0.0f };
                    glm::vec3 high{ 0.0f };
                    ComputeSkinnedBounds(*geometry, model->m_skeleton, skinOf->second,
                                         model->m_clips, 16, 0.05f, low, high);
                    gpuMesh->SetBounds(low, high);
                }

                meshes[&primitive] = gpuMesh.get();
                primitiveMaterials[&primitive] = primitive.material
                    ? materials.at(primitive.material)
                    : fallbackMaterial;
                model->m_meshes.push_back(std::move(gpuMesh));
            }
        }

        // A node moves if a clip targets it or any of its ancestors.
        std::vector<bool> targeted(data->nodes_count, false);
        for (const AnimationClip& clip : model->m_clips)
        {
            for (const AnimationChannel& channel : clip.channels)
            {
                targeted[channel.node] = true;
            }
        }
        const auto moves = [&](int node) {
            for (; node >= 0; node = model->m_skeleton.parents[node])
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
            // glTF: a skinned mesh ignores its node's transform; the joints
            // place it (the palette is in model space).
            const int skin = node.skin ? static_cast<int>(node.skin - data->skins) : -1;
            if (skin >= 0)
            {
                world = glm::mat4{ 1.0f };
            }

            for (cgltf_size p = 0; p < node.mesh->primitives_count; ++p)
            {
                const cgltf_primitive* primitive = &node.mesh->primitives[p];
                const auto found = meshes.find(primitive);
                if (found == meshes.end())
                {
                    continue;
                }

                const bool skinned = skin >= 0 && found->second->IsSkinned();
                model->m_parts.push_back(Part{
                    found->second,
                    primitiveMaterials.at(primitive),
                    world,
                    skinned ? -1 : animatedNode,
                    skinned ? skin : -1
                });
                model->m_animated = model->m_animated || skinned || animatedNode >= 0;
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

    int Model::FindNode(std::string_view name) const
    {
        const auto& names = m_skeleton.names;
        const auto found = std::find(names.begin(), names.end(), name);
        return found != names.end() ? static_cast<int>(found - names.begin()) : -1;
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

    void Model::SamplePose(int clip, float time, Pose& pose) const
    {
        pose = m_skeleton.rest;
        if (const AnimationClip* playing = GetClip(clip))
        {
            ApplyClip(*playing, time, pose);
        }
    }

    void Model::SamplePose(std::span<const ClipSample> samples, Pose& pose) const
    {
        // A running weighted average: each new pose is blended in by its
        // share of the weight seen so far, so n poses need n - 1 blends.
        pose = m_skeleton.rest;
        Pose sampled;
        float total = 0.0f;
        for (const ClipSample& sample : samples)
        {
            if (sample.weight <= 0.0f)
            {
                continue;
            }
            SamplePose(sample.clip, sample.time, sampled);
            if (sample.pinNode >= 0 && sample.pinNode < static_cast<int>(sampled.size()))
            {
                sampled[sample.pinNode].translation = m_skeleton.rest[sample.pinNode].translation;
            }
            total += sample.weight;
            if (total == sample.weight)
            {
                pose = sampled;
            }
            else
            {
                BlendPoses(pose, sampled, sample.weight / total, pose);
            }
        }
    }

    void Model::Submit(Renderer& renderer, const glm::mat4& transform, int clip, float time) const
    {
        if (!m_animated || (!IsSkinned() && !GetClip(clip)))
        {
            // Static: every part where it was baked.
            for (const Part& part : m_parts)
            {
                renderer.Submit(*part.mesh, m_materials[part.materialIndex], transform * part.transform);
            }
            return;
        }
        Pose pose;
        SamplePose(clip, time, pose);
        Submit(renderer, transform, pose);
    }

    void Model::Submit(Renderer& renderer, const glm::mat4& transform, const Pose& pose) const
    {
        const std::vector<glm::mat4> world = ComputeWorldMatrices(m_skeleton.parents, pose);

        // One palette per skin this frame, shared by all its parts.
        std::vector<std::uint32_t> palettes(m_skeleton.skins.size());
        std::vector<glm::mat4> palette;
        for (std::size_t s = 0; s < m_skeleton.skins.size(); ++s)
        {
            ComputePalette(m_skeleton.skins[s], world, palette);
            palettes[s] = renderer.AddPalette(palette);
        }

        for (const Part& part : m_parts)
        {
            const Material& material = m_materials[part.materialIndex];
            if (part.skin >= 0)
            {
                renderer.SubmitSkinned(*part.mesh, material, transform, palettes[part.skin]);
                continue;
            }
            const glm::mat4 partTransform = part.node >= 0 ? world[part.node] : part.transform;
            renderer.Submit(*part.mesh, material, transform * partTransform);
        }
    }
}
