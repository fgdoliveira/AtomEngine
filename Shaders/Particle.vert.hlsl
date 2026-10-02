// Camera-facing billboards, one instance per particle, six vertices each.
cbuffer ParticleUniforms : register(b0, space1)
{
    float4x4 u_viewProjection;
    float4 u_cameraRight; // xyz, w: atlas columns
    float4 u_cameraUp;
};

struct VSInput
{
    uint vertexId       : SV_VertexID;
    float4 positionSize : TEXCOORD0; // per instance
    float4 color        : TEXCOORD1;
    float4 params       : TEXCOORD2; // x: rotation, y: atlas cell
};

struct VSOutput
{
    float4 position      : SV_Position;
    float2 uv            : TEXCOORD0;
    float4 color         : TEXCOORD1;
    float3 worldPosition : TEXCOORD2;
    float  beamLit       : TEXCOORD3; // M45: seen only in the spot's beam (dust)
};

VSOutput main(VSInput input)
{
    // Two triangles: (0,1,2) (2,1,3) over corners in [-1,1]^2.
    static const float2 corners[6] = {
        float2(-1, -1), float2(1, -1), float2(-1, 1),
        float2(-1, 1), float2(1, -1), float2(1, 1)
    };
    const float2 corner = corners[input.vertexId];

    float sinR, cosR;
    sincos(input.params.x, sinR, cosR);
    const float2 rotated = float2(
        corner.x * cosR - corner.y * sinR,
        corner.x * sinR + corner.y * cosR);

    const float halfSize = input.positionSize.w * 0.5;
    const float3 world = input.positionSize.xyz
        + (u_cameraRight.xyz * rotated.x + u_cameraUp.xyz * rotated.y) * halfSize;

    const float columns = u_cameraRight.w;
    const float2 local = float2(corner.x * 0.5 + 0.5, 0.5 - corner.y * 0.5);

    VSOutput output;
    output.position = mul(u_viewProjection, float4(world, 1.0));
    output.uv = float2((input.params.y + local.x) / columns, local.y);
    output.color = input.color;
    output.worldPosition = world;
    output.beamLit = input.params.z;
    return output;
}
