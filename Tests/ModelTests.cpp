#include "Assets/Model.h"

#include <doctest/doctest.h>

#include <glm/common.hpp>

#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

using namespace Atom;

namespace
{
    const std::string Assets = ATOM_SOURCE_DIR "/Assets/";

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
        for (const auto& entry : std::filesystem::directory_iterator(Assets + folder))
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
    const auto primitives = LoadModelGeometry(Assets + "Street/street_col.glb");
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
    const auto primitives = LoadModelGeometry(Assets + "Interior/interior.glb");
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
        "atom_water_stain", "atom_grime", "atom_shop_sign", "atom_ofuda", "atom_road_diamond", "atom_road_paint" };
    int maskedSeen = 0;
    int decalsSeen = 0;
    for (const char* file : { "Street/street.glb", "Shrine/shrine.glb", "Kit/bush.glb", "Kit/machiya.glb",
                              "Interior/interior.glb", "Kit/shrine_gate.glb" })
    {
        const auto materials = LoadModelMaterials(Assets + file);
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

TEST_CASE("A missing model file loads no geometry")
{
    CHECK(LoadModelGeometry(Assets + "nope.glb").empty());
}
