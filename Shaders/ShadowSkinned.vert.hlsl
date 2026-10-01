// Shadow.vert for skinned meshes: the character casts its posed shadow.
#include "Skinning.hlsli"

cbuffer ObjectUniforms : register(b0, space1)
{
    float4x4 u_viewProjection;
    float4x4 u_model;
};

struct VSInput
{
    float3 position : TEXCOORD0;
    float2 uv       : TEXCOORD1;
    float4 color    : TEXCOORD2;
    uint4  joints   : TEXCOORD3; // second stream
    float4 weights  : TEXCOORD4;
};

struct VSOutput
{
    float4 position : SV_Position;
    float2 uv       : TEXCOORD0;
};

VSOutput main(VSInput input)
{
    const float4 skinned = mul(SkinMatrix(input.joints, input.weights), float4(input.position, 1.0));

    VSOutput output;
    output.position = mul(u_viewProjection, mul(u_model, skinned));
    output.uv = input.uv;
    return output;
}
