// Basic.vert for skinned meshes: the vertex is first moved by its joints
// (model space), then placed like any other. No wind sway on characters.
#include "Skinning.hlsli"

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
    float4 color    : TEXCOORD3;
    float2 lightmapUv : TEXCOORD4;
    uint4  joints   : TEXCOORD5; // second stream
    float4 weights  : TEXCOORD6;
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
    const float4x4 skin = SkinMatrix(input.joints, input.weights);
    const float4 skinned = mul(skin, float4(input.position, 1.0));
    const float3 skinnedNormal = mul((float3x3)skin, input.normal);

    const float4 worldPosition = mul(u_model, skinned);

    VSOutput output;
    output.position = mul(u_viewProjection, worldPosition);
    // Joints rotate and translate; the blend of rotations isn't quite a
    // rotation, so the fragment shader's normalize tidies the length.
    output.worldNormal = mul((float3x3)u_model, skinnedNormal);
    output.uv = input.uv;
    output.worldPosition = worldPosition.xyz;
    output.color = input.color;
    output.lightmapUv = input.lightmapUv;
    return output;
}
