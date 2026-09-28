// Fragment textures/samplers live in (t[n]/s[n], space2), uniforms in
// (b[n], space3) for SDL_GPU on D3D12.
#include "Common.hlsli"

Texture2D<float4> BaseColorTexture : register(t0, space2);
SamplerState BaseColorSampler : register(s0, space2);

Texture2D<float> ShadowMap : register(t1, space2);
SamplerComparisonState ShadowSampler : register(s1, space2);

Texture2D<float4> Lightmap : register(t2, space2);
SamplerState LightmapSampler : register(s2, space2);

// Per draw.
cbuffer MaterialUniforms : register(b0, space3)
{
    float4 u_baseColorFactor;
    float4 u_emissiveFactor; // w: baked-light weight (0 = none)
    float4 u_lightmap;       // x: intensity, y: weight (0 = no lightmap)
    float4 u_alpha;          // x: cutoff (0 = opaque), y: alpha-to-coverage
};

struct PSInput
{
    float4 position      : SV_Position;
    float3 worldNormal   : TEXCOORD0;
    float2 uv            : TEXCOORD1;
    float3 worldPosition : TEXCOORD2;
    float4 color         : TEXCOORD3; // baked light, linear
    float2 lightmapUv    : TEXCOORD4;
};

// 1 = fully lit, 0 = fully shadowed. The comparison sampler already blends
// 2x2 texels; a 3x3 grid of those gives soft, overcast-friendly edges.
float ComputeShadow(float3 worldPosition, float3 normal)
{
    if (u_shadowParams.x <= 0.0)
    {
        return 1.0;
    }

    // Push the lookup off the surface along its normal to avoid acne.
    const float3 offsetPosition = worldPosition + normal * u_shadowParams.w;
    const float4 lightPosition =
        mul(u_lightViewProjection, float4(offsetPosition, 1.0));
    const float3 ndc = lightPosition.xyz / lightPosition.w;

    // Clip space Y is up, texture V is down.
    const float2 uv = float2(ndc.x * 0.5 + 0.5, 0.5 - ndc.y * 0.5);
    if (any(uv < 0.0) || any(uv > 1.0) || ndc.z > 1.0)
    {
        return 1.0; // outside the shadow box: lit
    }

    const float texel = u_shadowParams.y;
    float visibility = 0.0;
    [unroll] for (int y = -1; y <= 1; ++y)
    {
        [unroll] for (int x = -1; x <= 1; ++x)
        {
            visibility += ShadowMap.SampleCmpLevelZero(
                ShadowSampler, uv + float2(x, y) * texel, ndc.z);
        }
    }
    return visibility / 9.0;
}

float4 main(PSInput input, bool frontFace : SV_IsFrontFace) : SV_Target0
{
    // Double-sided cards are lit from whichever side we see.
    const float3 normal = normalize(input.worldNormal) * (frontFace ? 1.0 : -1.0);
    float4 baseColor =
        BaseColorTexture.Sample(BaseColorSampler, input.uv) * u_baseColorFactor;

    // Alpha testing (M17). With MSAA, alpha-to-coverage: the alpha is
    // sharpened to a ~1-pixel ramp around the cutoff, and the hardware turns
    // it into covered samples - antialiased edges without sorting.
    if (u_alpha.x > 0.0)
    {
        if (u_alpha.y > 0.0)
        {
            baseColor.a = saturate((baseColor.a - u_alpha.x)
                / max(fwidth(baseColor.a), 0.0001) + 0.5);
        }
        else
        {
            clip(baseColor.a - u_alpha.x);
            baseColor.a = 1.0;
        }
    }

    const float shadow = ComputeShadow(input.worldPosition, normal);

    // Overcast: hemispheric sky/ground ambient plus a soft Lambert sun.
    // Shadows remove the sun and a share of the sky light, so they stay
    // visible under weak, diffuse sunlight.
    const float hemisphere = normal.y * 0.5 + 0.5;
    const float3 hemisphereAmbient =
        lerp(u_groundColor.rgb, u_skyColor.rgb, hemisphere);
    // Baked (M15): the vertex colour is how much of a white sky, plus its
    // bounces, reaches this point, so it already holds the orientation and
    // the occlusion the hemisphere term only approximates.
    const float3 bakedAmbient = u_skyColor.rgb * input.color.rgb;
    float3 ambient = lerp(hemisphereAmbient, bakedAmbient, u_emissiveFactor.w);
    // Lightmap (M16): baked direct + bounced light of the level's own
    // lights, resolved across each face. Replaces the ambient estimate.
    const float3 lightmap =
        Lightmap.Sample(LightmapSampler, input.lightmapUv).rgb * u_lightmap.x;
    ambient = lerp(ambient, lightmap, u_lightmap.y)
        * lerp(1.0, shadow, u_shadowParams.z);
    // Thin alpha-tested cards (leaves, cloth) let light through: lit from
    // behind they glow at half strength instead of turning black.
    const float facing = dot(normal, u_sunDirection.xyz);
    const float sunLight = u_alpha.x > 0.0
        ? max(saturate(facing), 0.5 * saturate(-facing))
        : saturate(facing);
    const float3 sun = sunLight * u_sunColor.rgb * shadow;

    const float3 lit = baseColor.rgb * (ambient + sun);
    const float3 emitted = baseColor.rgb * u_emissiveFactor.rgb;

    const float fog = ComputeFog(input.worldPosition);
    const float3 color = lerp(lit + emitted, u_fogColor.rgb, fog);

    return float4(color, baseColor.a);
}
