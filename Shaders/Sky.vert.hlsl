// Night sky (M23): one fullscreen triangle on the far plane. Each pixel's
// view direction is rebuilt from its clip position with the inverse
// view-projection, so the panorama stays fixed in the world as you turn.
cbuffer SkyUniforms : register(b0, space1)
{
    float4x4 u_inverseViewProjection; // camera at the origin: rotation only
};

struct VSOutput
{
    float4 position  : SV_Position;
    float3 direction : TEXCOORD0;
};

VSOutput main(uint vertexId : SV_VertexID)
{
    const float2 uv = float2((vertexId << 1) & 2, vertexId & 2);
    const float2 clip = float2(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0);

    VSOutput output;
    // z = w: depth 1, the far plane - behind everything the scene draws.
    output.position = float4(clip, 1.0, 1.0);
    const float4 world = mul(u_inverseViewProjection, float4(clip, 1.0, 1.0));
    output.direction = world.xyz / world.w;
    return output;
}
