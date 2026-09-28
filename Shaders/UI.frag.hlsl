// Glyph atlases are white with coverage in alpha; solid rectangles sample a
// white texel. Either way: output = vertex colour x texel.
Texture2D<float4> UITexture : register(t0, space2);
SamplerState UISampler : register(s0, space2);

struct PSInput
{
    float4 position : SV_Position;
    float2 uv       : TEXCOORD0;
    float4 color    : TEXCOORD1;
};

float4 main(PSInput input) : SV_Target0
{
    return UITexture.Sample(UISampler, input.uv) * input.color;
}
