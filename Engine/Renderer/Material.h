#pragma once

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

namespace Atom
{
    class Texture;

    // Opaque: every pixel drawn. Mask: pixels whose alpha is below
    // alphaCutoff are discarded (alpha testing: leaves, grass, torn cloth,
    // chain-link) - no sorting needed, depth still written. Blend: a decal
    // (stains, signs, road markings) blended over the surface it lies on,
    // drawn after everything else without writing depth.
    enum class AlphaMode
    {
        Opaque,
        Mask,
        Blend,
    };

    struct Material
    {
        // nullptr uses the renderer's 1x1 white texture.
        const Texture* baseColorTexture = nullptr;
        glm::vec4 baseColorFactor{ 1.0f };

        // Emission is the base color scaled by this factor. Kit assets drive
        // emission from the same image as the base color, so a separate
        // emissive texture is not needed yet.
        glm::vec3 emissiveFactor{ 0.0f };
        // Which pixels glow (M23). Without one, the base colour glows.
        const Texture* emissiveTexture = nullptr;

        // Share of the runtime fog applied (M23): 1 = all, 0 = none. Lights
        // (signs, lamps) cut through fog; far cards get a reduced amount.
        float fogAmount = 1.0f;

        // Wet surface (M25), 0..1: the emitted light (sign reflections)
        // ripples with animated noise and the surface catches a faint
        // moving sheen. glTF extras "atom_wet".
        float wet = 0.0f;

        // Baked light texture (M16), mapped by the mesh's second UV set.
        // When present it replaces the ambient term; intensity scales it.
        const Texture* lightmap = nullptr;
        float lightmapIntensity = 1.0f;

        // Surface response to the spot light (M42): glTF roughness (0
        // smooth .. 1 rough) sets how tight the highlight is; "atom_specular"
        // (glTF extras) how strong, -1 = from the roughness.
        float roughness = 1.0f;
        float specular = -1.0f;
        // Revealed by light (M44, "atom_reveal"): a decal whose alpha follows
        // the spot's reach - chalk only the flashlight shows.
        float reveal = 0.0f;
        // Water (M48, "atom_water"): drawn by the water shader after the
        // opaque scene, blended; the mesh's first UV's u is the depth below
        // it (0 at the shore, 1 deep), which drives tint, foam and alpha.
        float water = 0.0f;

        // Toon shading (M79): the sun's light in three steps (the toon
        // shader variant), as DRIFT's three.js original draws everything.
        bool toon = false;
        // Inverted-hull outline (M79): when > 0, the mesh is drawn a second
        // time pushed out this far along its normals (model units), front
        // faces culled, in outlineColor (linear) - a rim round the silhouette.
        float outline = 0.0f;
        glm::vec3 outlineColor{ 0.0f };

        AlphaMode alphaMode = AlphaMode::Opaque;
        float alphaCutoff = 0.5f;
        bool doubleSided = false; // no back-face culling (thin cards)
    };
}
