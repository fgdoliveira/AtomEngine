// Fragment textures/samplers live in (t[n]/s[n], space2), uniforms in
// (b[n], space3) for SDL_GPU on D3D12.
#include "Common.hlsli"

Texture2D<float4> BaseColorTexture : register(t0, space2);
SamplerState BaseColorSampler : register(s0, space2);

Texture2D<float> ShadowMap : register(t1, space2);
SamplerComparisonState ShadowSampler : register(s1, space2);

// Per draw.
cbuffer MaterialUniforms : register(b0, space3)
{
    float4 u_baseColorFactor;
    float4 u_emissiveFactor; // w: baked-light weight (0 = none)
};

struct PSInput
{
    float4 position      : SV_Position;
    float3 worldNormal   : TEXCOORD0;
    float2 uv            : TEXCOORD1;
    float3 worldPosition : TEXCOORD2;
    float4 color         : TEXCOORD3; // baked light, linear
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

float4 main(PSInput input) : SV_Target0
{
    const float3 normal = normalize(input.worldNormal);
    const float4 baseColor =
        BaseColorTexture.Sample(BaseColorSampler, input.uv) * u_baseColorFactor;

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
    const float3 ambient = lerp(hemisphereAmbient, bakedAmbient, u_emissiveFactor.w)
        * lerp(1.0, shadow, u_shadowParams.z);
    const float3 sun =
        saturate(dot(normal, u_sunDirection.xyz)) * u_sunColor.rgb * shadow;

    const float3 lit = baseColor.rgb * (ambient + sun);
    const float3 emitted = baseColor.rgb * u_emissiveFactor.rgb;

    const float fog = ComputeFog(input.worldPosition);
    const float3 color = lerp(lit + emitted, u_fogColor.rgb, fog);

    return float4(color, baseColor.a);
}
