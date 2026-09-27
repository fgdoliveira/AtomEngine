// Depth-only pass from the sun. Same layout as ObjectUniforms, with the
// light's view-projection in place of the camera's.
cbuffer ObjectUniforms : register(b0, space1)
{
    float4x4 u_viewProjection;
    float4x4 u_model;
};

float4 main(float3 position : TEXCOORD0) : SV_Position
{
    return mul(u_viewProjection, mul(u_model, float4(position, 1.0)));
}
