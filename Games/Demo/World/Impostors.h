#pragma once

#include "Renderer/Material.h"
#include "Renderer/Mesh.h"
#include "Renderer/Texture.h"

#include <glm/vec3.hpp>

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Atom
{
    class Renderer;
}

namespace AtomGame
{
    // Impostors (M24): a building pre-rendered from several directions into
    // one atlas (Tools/Blender/atom_city.py). At runtime a card at the
    // building's centre turns to face the camera and shows the view closest
    // to the direction it's seen from. Far away, a flat card is
    // indistinguishable from the geometry, at the cost of one quad.

    struct ImpostorDescriptor
    {
        std::string atlas; // file name, next to the descriptor
        int views = 8;     // around the building, from its front toward +X
        float width = 1.0f;
        float height = 1.0f;
        float fog = 1.0f;  // share of the runtime fog
    };

    // Parses <name>.json; nullopt (and `error`) when it's malformed.
    std::optional<ImpostorDescriptor> ParseImpostorDescriptor(std::string_view text, std::string& error);

    // Which view to show for a camera seen at `angle` radians from the
    // building's front (toward +X). The current view is kept until the
    // camera is more than `hysteresis` past the boundary to the next one,
    // so a card on the edge doesn't flicker between two images. current < 0
    // picks the nearest view.
    int SelectImpostorView(int current, float angle, int views, float hysteresis);

    // One building's atlas, as a mesh per view (the card, with that view's
    // part of the atlas) and the material they share.
    class ImpostorSet
    {
    public:
        static std::unique_ptr<ImpostorSet> Load(Atom::Renderer& renderer, const std::string& descriptorPath);

        const ImpostorDescriptor& GetDescriptor() const { return m_descriptor; }
        const Atom::Mesh& GetView(int view) const { return *m_views[static_cast<std::size_t>(view)]; }
        const Atom::Material& GetMaterial() const { return m_material; }

    private:
        ImpostorDescriptor m_descriptor;
        std::unique_ptr<Atom::Texture> m_atlas;
        std::vector<std::unique_ptr<Atom::Mesh>> m_views;
        Atom::Material m_material;
    };
}
