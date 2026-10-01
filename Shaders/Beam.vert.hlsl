// The flashlight's fake beam (M45): three planes crossing along the spot's
// axis, placed by the spot's model matrix. No volume is computed - each
// plane just shows how much of the spot's light passes through it.
cbuffer ObjectUniforms : register(b0, space1)
{
    float4x4 u_viewProjection;
    float4x4 u_model;
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
    float3 worldNormal   : TEXCOORD1;
};

VSOutput main(VSInput input)
{
    const float4 world = mul(u_model, float4(input.position, 1.0));
    VSOutput output;
    output.position = mul(u_viewProjection, world);
    output.worldPosition = world.xyz;
    output.worldNormal = mul((float3x3)u_model, input.normal);
    return output;
}
