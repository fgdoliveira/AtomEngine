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
    float4 color = texel * input.color;

    // Same fog as the scene: particles in the distance melt into it too.
    color.rgb = lerp(color.rgb, u_fogColor.rgb, ComputeFog(input.worldPosition));
    return color;
}
