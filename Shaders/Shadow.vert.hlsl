// Depth-only pass from the sun. Same layout as ObjectUniforms, with the
// light's view-projection in place of the camera's.
#include "Sway.hlsli"

cbuffer ObjectUniforms : register(b0, space1)
{
    float4x4 u_viewProjection;
    float4x4 u_model;
};

struct VSInput
{
    float3 position : TEXCOORD0;
    float2 uv       : TEXCOORD1;
    float4 color    : TEXCOORD2; // alpha: 1 - sway weight
};

struct VSOutput
{
    float4 position : SV_Position;
    float2 uv       : TEXCOORD0; // for alpha-tested materials
};

VSOutput main(VSInput input)
{
    // Swaying cards cast swaying shadows.
    float4 worldPosition = mul(u_model, float4(input.position, 1.0));
    worldPosition.xyz = ApplySway(worldPosition.xyz, 1.0 - input.color.a);

    VSOutput output;
    output.position = mul(u_viewProjection, worldPosition);
    output.uv = input.uv;
    return output;
}
