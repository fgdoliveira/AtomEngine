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

        // Baked light texture (M16), mapped by the mesh's second UV set.
        // When present it replaces the ambient term; intensity scales it.
        const Texture* lightmap = nullptr;
        float lightmapIntensity = 1.0f;

        AlphaMode alphaMode = AlphaMode::Opaque;
        float alphaCutoff = 0.5f;
        bool doubleSided = false; // no back-face culling (thin cards)
    };
}
