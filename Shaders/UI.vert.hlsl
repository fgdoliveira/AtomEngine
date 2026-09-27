// 2D overlay: vertices arrive in window pixels, origin top-left.
cbuffer UIUniforms : register(b0, space1)
{
    float4 u_viewport; // xy: target size in pixels
};

struct VSInput
{
    float2 position : TEXCOORD0;
    float2 uv       : TEXCOORD1;
    float4 color    : TEXCOORD2;
};

struct VSOutput
{
    float4 position : SV_Position;
    float2 uv       : TEXCOORD0;
    float4 color    : TEXCOORD1;
};

VSOutput main(VSInput input)
{
    // Pixels -> clip space; Y flips because pixel rows grow downward.
    const float2 ndc = float2(
        input.position.x / u_viewport.x * 2.0 - 1.0,
        1.0 - input.position.y / u_viewport.y * 2.0);

    VSOutput output;
    output.position = float4(ndc, 0.0, 1.0);
    output.uv = input.uv;
    output.color = input.color;
    return output;
}
