#pragma once

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

namespace Atom
{
    class Texture;

    struct Material
    {
        // nullptr uses the renderer's 1x1 white texture.
        const Texture* baseColorTexture = nullptr;
        glm::vec4 baseColorFactor{ 1.0f };

        // Emission is the base color scaled by this factor. Kit assets drive
        // emission from the same image as the base color, so a separate
        // emissive texture is not needed yet.
        glm::vec3 emissiveFactor{ 0.0f };
    };
}
