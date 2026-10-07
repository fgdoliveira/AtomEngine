// Inverted-hull outline (M79): a flat colour, fogged like the scene so far
// outlines fade with what they surround.
#include "Common.hlsli"

cbuffer OutlineColor : register(b0, space3)
{
    float4 u_color; // rgb linear, w: share of the fog applied
};

struct PSInput
{
    float4 position      : SV_Position;
    float3 worldPosition : TEXCOORD0;
};

float4 main(PSInput input) : SV_Target
{
    const float fog = ComputeFog(input.worldPosition) * u_color.w;
    return float4(lerp(u_color.rgb, u_fogColor.rgb, fog), 1.0);
}
