// One oversized triangle covering the screen; no vertex buffer needed.

struct VSOutput
{
    float4 position : SV_Position;
    float2 uv       : TEXCOORD0;
};

VSOutput main(uint vertexId : SV_VertexID)
{
    // (0,0), (2,0), (0,2) in UV space -> covers [0,1]^2 after clipping.
    const float2 uv = float2((vertexId << 1) & 2, vertexId & 2);

    VSOutput output;
    // Texture V grows downward, clip-space Y upward.
    output.position = float4(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0, 0.0, 1.0);
    output.uv = uv;
    return output;
}
