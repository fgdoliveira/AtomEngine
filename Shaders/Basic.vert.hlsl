// Vertex uniforms live in (b[n], space1) for SDL_GPU on D3D12.
#include "Sway.hlsli"

cbuffer ObjectUniforms : register(b0, space1)
{
    float4x4 u_viewProjection;
    float4x4 u_model;
};

struct VSInput
{
    float3 position : TEXCOORD0;
    float3 normal   : TEXCOORD1;
    float2 uv       : TEXCOORD2;
    float4 color    : TEXCOORD3; // baked light, linear
    float2 lightmapUv : TEXCOORD4;
};

struct VSOutput
{
    float4 position      : SV_Position;
    float3 worldNormal   : TEXCOORD0;
    float2 uv            : TEXCOORD1;
    float3 worldPosition : TEXCOORD2;
    float4 color         : TEXCOORD3;
    float2 lightmapUv    : TEXCOORD4;
};

VSOutput main(VSInput input)
{
    float4 worldPosition = mul(u_model, float4(input.position, 1.0));
    worldPosition.xyz = ApplySway(worldPosition.xyz, 1.0 - input.color.a);

    VSOutput output;
    output.position = mul(u_viewProjection, worldPosition);
    // Model matrices are rigid + uniform scale for now; revisit with the
    // inverse-transpose once non-uniform scaling shows up.
    output.worldNormal = mul((float3x3)u_model, input.normal);
    output.uv = input.uv;
    output.worldPosition = worldPosition.xyz;
    output.color = input.color;
    output.lightmapUv = input.lightmapUv;
    return output;
}
