// Glow, steps 2 and 3: one direction of a separable Gaussian blur. Run
// once horizontally and once vertically, it equals the 2D blur with
// 9 + 9 taps instead of 81. Linear filtering lets each tap between two
// texels read both at once (weights and offsets pre-combined).
Texture2D<float4> SourceTexture : register(t0, space2);
SamplerState SourceSampler : register(s0, space2);

cbuffer GlowBlur : register(b0, space3)
{
    float4 u_direction; // xy: one texel along the blur direction
};

struct PSInput
{
    float4 position : SV_Position;
    float2 uv       : TEXCOORD0;
};

float4 main(PSInput input) : SV_Target0
{
    // 17-tap Gaussian (sigma ~4) folded into 9 bilinear samples.
    static const float offsets[5] = { 0.0, 1.4588, 3.4048, 5.3514, 7.3006 };
    static const float weights[5] = { 0.1406, 0.2355, 0.1563, 0.0659, 0.0172 };

    float3 color = SourceTexture.Sample(SourceSampler, input.uv).rgb * weights[0];
    [unroll] for (int i = 1; i < 5; ++i)
    {
        const float2 step = u_direction.xy * offsets[i];
        color += SourceTexture.Sample(SourceSampler, input.uv + step).rgb * weights[i];
        color += SourceTexture.Sample(SourceSampler, input.uv - step).rgb * weights[i];
    }
    // The rounded weights sum to slightly more than 1: normalise so the
    // blur neither brightens nor darkens.
    const float total = weights[0] + 2.0 * (weights[1] + weights[2] + weights[3] + weights[4]);
    return float4(color / total, 1.0);
}
