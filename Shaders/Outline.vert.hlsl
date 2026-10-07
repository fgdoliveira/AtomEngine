// Inverted-hull outline (M79): the mesh again, each vertex pushed out along
// its normal in model space, drawn with front faces culled - only the
// shell's inside shows, as a rim around the silhouette. The three.js
// original's toon.js does exactly this (position + normal * width).
cbuffer ObjectUniforms : register(b0, space1)
{
    float4x4 u_viewProjection;
    float4x4 u_model;
};

cbuffer OutlineUniforms : register(b2, space1)
{
    float4 u_outline; // x: width in model units
};

struct VSInput
{
    float3 position : TEXCOORD0;
    float3 normal   : TEXCOORD1;
};

struct VSOutput
{
    float4 position      : SV_Position;
    float3 worldPosition : TEXCOORD0;
};

VSOutput main(VSInput input)
{
    const float4 world = mul(u_model, float4(input.position + input.normal * u_outline.x, 1.0));
    VSOutput output;
    output.position = mul(u_viewProjection, world);
    output.worldPosition = world.xyz;
    return output;
}
