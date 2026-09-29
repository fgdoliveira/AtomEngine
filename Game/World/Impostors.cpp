#include "World/Impostors.h"

#include "Level/JsonText.h"
#include "Renderer/Renderer.h"

#include <glm/gtc/constants.hpp>

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

namespace AtomGame
{
    std::optional<ImpostorDescriptor> ParseImpostorDescriptor(std::string_view text, std::string& error)
    {
        nlohmann::json root;
        if (error = ParseJsonText(text, root); !error.empty())
        {
            return std::nullopt;
        }
        ImpostorDescriptor d;
        d.atlas = root.value("atlas", "");
        d.views = root.value("views", 0);
        d.width = root.value("width", 0.0f);
        d.height = root.value("height", 0.0f);
        d.fog = root.value("fog", 1.0f);
        if (d.atlas.empty() || d.views < 1 || d.width <= 0.0f || d.height <= 0.0f)
        {
            error = "an impostor needs \"atlas\", \"views\" >= 1 and a positive \"width\" and \"height\"";
            return std::nullopt;
        }
        return d;
    }

    int SelectImpostorView(int current, float angle, int views, float hysteresis)
    {
        const float step = glm::two_pi<float>() / static_cast<float>(views);
        const auto wrapped = [](float a) {
            return std::remainder(a, glm::two_pi<float>()); // -pi..pi
        };
        const int nearest = static_cast<int>(std::lround(wrapped(angle) / step) % views + views) % views;
        if (current < 0 || current >= views)
        {
            return nearest;
        }
        // Keep the current view while the camera is within half a step of
        // its direction plus the hysteresis margin.
        const float offset = std::abs(wrapped(angle - step * static_cast<float>(current)));
        return offset <= step * 0.5f + hysteresis ? current : nearest;
    }

    std::unique_ptr<ImpostorSet> ImpostorSet::Load(Atom::Renderer& renderer, const std::string& descriptorPath)
    {
        std::ifstream file(descriptorPath, std::ios::binary);
        if (!file)
        {
            std::cerr << "Cannot open impostor '" << descriptorPath << "'\n";
            return nullptr;
        }
        std::stringstream text;
        text << file.rdbuf();
        std::string error;
        const auto descriptor = ParseImpostorDescriptor(text.str(), error);
        if (!descriptor)
        {
            std::cerr << "Impostor '" << descriptorPath << "': " << error << '\n';
            return nullptr;
        }

        auto set = std::unique_ptr<ImpostorSet>(new ImpostorSet());
        set->m_descriptor = *descriptor;
        const std::filesystem::path atlas = std::filesystem::path(descriptorPath).parent_path() / descriptor->atlas;
        set->m_atlas = renderer.LoadTexture(atlas.string());
        if (!set->m_atlas)
        {
            return nullptr;
        }

        // The atlas holds the finished look (lit windows included): it is the
        // emitted colour, over a black base whose alpha does the cut-out.
        Atom::Material& m = set->m_material;
        m.baseColorTexture = set->m_atlas.get();
        m.baseColorFactor = glm::vec4{ 0.0f, 0.0f, 0.0f, 1.0f };
        m.emissiveTexture = set->m_atlas.get();
        m.emissiveFactor = glm::vec3{ 1.0f };
        m.alphaMode = Atom::AlphaMode::Mask;
        m.alphaCutoff = 0.5f;
        m.fogAmount = descriptor->fog;

        // One card per view: bottom centre at the origin, facing +Z, with
        // that view's column of the atlas (rows top first: v = 0 at the top).
        const float w = descriptor->width * 0.5f;
        const float h = descriptor->height;
        for (int view = 0; view < descriptor->views; ++view)
        {
            const float u0 = static_cast<float>(view) / static_cast<float>(descriptor->views);
            const float u1 = static_cast<float>(view + 1) / static_cast<float>(descriptor->views);
            const glm::vec3 normal{ 0.0f, 0.0f, 1.0f };
            const Atom::Vertex vertices[4]{
                { { -w, 0.0f, 0.0f }, normal, { u0, 1.0f } },
                { { w, 0.0f, 0.0f }, normal, { u1, 1.0f } },
                { { w, h, 0.0f }, normal, { u1, 0.0f } },
                { { -w, h, 0.0f }, normal, { u0, 0.0f } },
            };
            const std::uint32_t indices[6]{ 0, 1, 2, 0, 2, 3 };
            auto mesh = renderer.CreateMesh(vertices, indices);
            if (!mesh)
            {
                return nullptr;
            }
            set->m_views.push_back(std::move(mesh));
        }
        std::cout << "Loaded impostor '" << descriptorPath << "' (" << descriptor->views << " views)\n";
        return set;
    }
}
