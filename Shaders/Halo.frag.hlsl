// Halos (M23): soft glows around lamps and signs, added onto the scene
// (additive blending: light only ever brightens). Same billboards as the
// particles; fog dims them instead of tinting them, since fogged light
// fades rather than turning grey.
#include "Common.hlsli"

Texture2D<float4> AtlasTexture : register(t0, space2);
SamplerState AtlasSampler : register(s0, space2);

struct PSInput
{
    float4 position      : SV_Position;
    float2 uv            : TEXCOORD0;
    float4 color         : TEXCOORD1;
    float3 worldPosition : TEXCOORD2;
};

float4 main(PSInput input) : SV_Target0
{
    const float4 texel = AtlasTexture.Sample(AtlasSampler, input.uv);
    const float strength = texel.a * input.color.a;
    const float fogged = 1.0 - 0.7 * ComputeFog(input.worldPosition);
    // Blended as src * a + dst (see the halo pipeline).
    return float4(input.color.rgb * fogged, strength);
}
