// The sky, behind everything. Two kinds:
// - Night sky (M23): an equirectangular panorama - longitude across the
//   texture, latitude down it - sampled by view direction. It sits at
//   infinity: no fog, no parallax; the level's fog colour should match the
//   panorama's horizon so the ground fades into it.
// - Day sky (M47): a gradient from horizon to zenith with the sun (see
//   SkyGradient.hlsli). The same for the whole draw, so the branch is free.
#include "SkyGradient.hlsli"

Texture2D<float4> Panorama : register(t0, space2);
SamplerState PanoramaSampler : register(s0, space2);

cbuffer SkyParams : register(b0, space3)
{
    float4 u_sky;          // x: intensity, y: 1 = gradient, 0 = panorama
    float4 u_skyZenith;    // rgb
    float4 u_skyHorizon;   // rgb, w: cosine of the sun disc's radius
    float4 u_sunDirection; // xyz towards the sun, w: halo strength
    float4 u_sunColor;     // rgb
};

struct PSInput
{
    float4 position  : SV_Position;
    float3 direction : TEXCOORD0;
};

static const float Pi = 3.14159265;

float4 main(PSInput input) : SV_Target0
{
    const float3 d = normalize(input.direction);
    float3 color;
    [branch] if (u_sky.y > 0.5)
    {
        color = SkyGradient(d, u_skyZenith.rgb, u_skyHorizon.rgb,
                            u_sunDirection.xyz, u_sunColor.rgb, u_skyHorizon.w, u_sunDirection.w);
    }
    else
    {
        // atan2 in [-pi, pi] around the vertical axis, asin for the height.
        const float u = atan2(d.x, -d.z) / (2.0 * Pi) + 0.5;
        const float v = 0.5 - asin(clamp(d.y, -1.0, 1.0)) / Pi;
        // Explicit gradient-free lod 0: the atan2 seam would otherwise pick a
        // tiny mip along one pixel column.
        color = Panorama.SampleLevel(PanoramaSampler, float2(u, v), 0).rgb;
    }
    return float4(color * u_sky.x, 1.0);
}
