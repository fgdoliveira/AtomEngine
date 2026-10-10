#include "Assets/Model.h"
#include "TestAssets.h" // v0.0.14: source assets in two roots

#include <doctest/doctest.h>

#include <glm/common.hpp>

#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

using namespace Atom;

namespace
{

    float Luminance(const Vertex& vertex)
    {
        return std::max({ vertex.color[0], vertex.color[1], vertex.color[2] }) / 65535.0f;
    }
}

TEST_CASE("Every shipped visual model carries baked light")
{
    int models = 0;
    for (const char* folder : { "Kit", "Street", "Shrine", "Interior" })
    {
        for (const auto& entry : std::filesystem::directory_iterator(AtomTests::Asset(folder)))
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
    }
    CHECK(models >= 17);
}

TEST_CASE("Models without COLOR_0 read as white and unbaked")
{
    // Collision proxies are exported without colours.
    const auto primitives = LoadModelGeometry(AtomTests::Asset("Street/street_col.glb"));
    REQUIRE_FALSE(primitives.empty());
    for (const PrimitiveGeometry& primitive : primitives)
    {
        CHECK_FALSE(primitive.hasBakedLight);
        REQUIRE_FALSE(primitive.vertices.empty());
        CHECK(Luminance(primitive.vertices.front()) == doctest::Approx(1.0f));
    }
}

TEST_CASE("The interior carries lightmap UVs inside the texture")
{
    const auto primitives = LoadModelGeometry(AtomTests::Asset("Interior/interior.glb"));
    REQUIRE_FALSE(primitives.empty());
    for (const PrimitiveGeometry& primitive : primitives)
    {
        REQUIRE(primitive.hasLightmapUv);
        glm::vec2 low{ 1.0f };
        glm::vec2 high{ 0.0f };
        for (const Vertex& vertex : primitive.vertices)
        {
            low = glm::min(low, vertex.lightmapUv);
            high = glm::max(high, vertex.lightmapUv);
        }
        CHECK(low.x >= 0.0f);
        CHECK(low.y >= 0.0f);
        CHECK(high.x <= 1.0f);
        CHECK(high.y <= 1.0f);
        CHECK(high.x > low.x); // not collapsed to a point
    }
}

TEST_CASE("Masks ship double-sided, decals as blend, everything else opaque")
{
    // Tools/Blender/atom_kit.py: MASKED and DECALS.
    const std::vector<std::string> masked{ "atom_leaves", "atom_grass", "atom_noren", "atom_chain_link" };
    const std::vector<std::string> decals{
        "atom_water_stain", "atom_grime", "atom_shop_sign", "atom_ofuda", "atom_road_diamond", "atom_road_paint",
        "atom_reveal_marks" };
    int maskedSeen = 0;
    int decalsSeen = 0;
    for (const char* file : { "Street/street.glb", "Street/street_west.glb", "Street/street_centre.glb",
                              "Street/street_east.glb", "Shrine/shrine.glb", "Kit/bush.glb", "Kit/machiya.glb",
                              "Interior/interior.glb", "Kit/shrine_gate.glb" })
    {
        const auto materials = LoadModelMaterials(AtomTests::Asset(file));
        REQUIRE_FALSE(materials.empty());
        for (const MaterialInfo& material : materials)
        {
            INFO(file << ": " << material.name);
            const bool shouldMask =
                std::find(masked.begin(), masked.end(), material.name) != masked.end();
            const bool isDecal =
                std::find(decals.begin(), decals.end(), material.name) != decals.end();
            if (isDecal)
            {
                ++decalsSeen;
                CHECK(material.alphaMode == AlphaMode::Blend);
            }
            else if (shouldMask)
            {
                ++maskedSeen;
                CHECK(material.alphaMode == AlphaMode::Mask);
                CHECK(material.alphaCutoff == doctest::Approx(0.5f));
                CHECK(material.doubleSided);
            }
            else
            {
                CHECK(material.alphaMode == AlphaMode::Opaque);
            }
        }
    }
    CHECK(maskedSeen >= 7);
    CHECK(decalsSeen >= 10);
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

TEST_CASE("A missing model file loads no geometry")
{
    CHECK(LoadModelGeometry(AtomTests::Asset("nope.glb")).empty());
}
