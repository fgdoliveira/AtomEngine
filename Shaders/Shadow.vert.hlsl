// Depth-only pass from the sun. Same layout as ObjectUniforms, with the
// light's view-projection in place of the camera's.
cbuffer ObjectUniforms : register(b0, space1)
{
    float4x4 u_viewProjection;
    float4x4 u_model;
};

struct VSInput
{
    float3 position : TEXCOORD0;
    float2 uv       : TEXCOORD1;
};

struct VSOutput
{
    float4 position : SV_Position;
    float2 uv       : TEXCOORD0; // for alpha-tested materials
};

VSOutput main(VSInput input)
{
    VSOutput output;
    output.position = mul(u_viewProjection, mul(u_model, float4(input.position, 1.0)));
    output.uv = input.uv;
    return output;
}
