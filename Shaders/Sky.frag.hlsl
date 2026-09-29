// Night sky (M23): an equirectangular panorama - longitude across the
// texture, latitude down it - sampled by view direction. It sits at
// infinity: no fog, no parallax; the level's fog colour should match the
// panorama's horizon so the ground fades into it.
Texture2D<float4> Panorama : register(t0, space2);
SamplerState PanoramaSampler : register(s0, space2);

cbuffer SkyParams : register(b0, space3)
{
    float4 u_sky; // x: intensity
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
    // atan2 in [-pi, pi] around the vertical axis, asin for the height.
    const float u = atan2(d.x, -d.z) / (2.0 * Pi) + 0.5;
    const float v = 0.5 - asin(clamp(d.y, -1.0, 1.0)) / Pi;
    // Explicit gradient-free lod 0: the atan2 seam would otherwise pick a
    // tiny mip along one pixel column.
    const float3 color = Panorama.SampleLevel(PanoramaSampler, float2(u, v), 0).rgb;
    return float4(color * u_sky.x, 1.0);
}
