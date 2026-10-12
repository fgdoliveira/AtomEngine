#include "Assets/Model.h"
#include "TestAssets.h"

#include <doctest/doctest.h>

#include <algorithm>
#include <filesystem>
#include <string>

using namespace Atom;

namespace
{
    float Luminance(const Vertex& vertex)
    {
        return std::max({ vertex.color[0], vertex.color[1], vertex.color[2] }) / 65535.0f;
    }
}

// Shipped files checked: the "content" suite (Content.<owner> in ctest).
TEST_SUITE_BEGIN("content");

TEST_CASE("Every kit model carries baked light")
{
    int models = 0;
    for (const auto& entry : std::filesystem::directory_iterator(AtomTests::Asset("Kit")))
    {
        const std::string name = entry.path().filename().string();
        if (entry.path().extension() != ".glb" || name.find("_col") != std::string::npos)
        {
            continue;
        }
        ++models;
        INFO(name);

        const auto primitives = LoadModelGeometry(entry.path().string());
        REQUIRE_FALSE(primitives.empty());

        float darkest = 1.0f;
        float brightest = 0.0f;
        for (const PrimitiveGeometry& primitive : primitives)
        {
            CHECK(primitive.hasBakedLight);
            for (const Vertex& vertex : primitive.vertices)
            {
                darkest = std::min(darkest, Luminance(vertex));
                brightest = std::max(brightest, Luminance(vertex));
            }
        }
        // A real bake has both occluded and open vertices - except a
        // loose card piece (a grass tuft) alone under the sky.
        CHECK(brightest > 0.0f);
        const auto materials = LoadModelMaterials(entry.path().string());
        const bool onlyCards = std::all_of(materials.begin(), materials.end(),
            [](const MaterialInfo& material) { return material.alphaMode == AlphaMode::Mask; });
        if (!onlyCards)
        {
            CHECK(darkest < brightest);
        }
    }
    CHECK(models >= 30);
}

TEST_CASE("Night lights ship an emissive mask and cut through fog")
{
    for (const char* file : { "Kit/neon_sign.glb", "Kit/street_lamp.glb" })
    {
        INFO(file);
        int lights = 0;
        for (const MaterialInfo& material : LoadModelMaterials(AtomTests::Asset(file)))
        {
            INFO(material.name);
            if (material.name == "atom_neon_sign" || material.name == "atom_lamp_glass")
            {
                ++lights;
                CHECK(material.hasEmissiveTexture);  // a mask, not the base colour
                CHECK(material.fogAmount < 1.0f);     // "atom_fog" from the glTF extras
            }
            else
            {
                CHECK(material.fogAmount == doctest::Approx(1.0f));
            }
        }
        CHECK(lights == 1);
    }
    // Older glowing pieces keep glowing by their base colour.
    for (const MaterialInfo& material : LoadModelMaterials(AtomTests::Asset("Kit/vending_machine.glb")))
    {
        CHECK(material.fogAmount == doctest::Approx(1.0f));
    }
}

TEST_SUITE_END();
