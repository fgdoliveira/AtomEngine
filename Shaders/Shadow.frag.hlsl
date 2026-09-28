// Depth-only: the rasterizer writes depth, no colour output is needed.
// Alpha-tested materials (M17) discard the same pixels as in the scene,
// so leaves and chain-link cast their own shape, not a solid card.
Texture2D<float4> BaseColorTexture : register(t0, space2);
SamplerState BaseColorSampler : register(s0, space2);

cbuffer ShadowMaterial : register(b0, space3)
{
    float4 u_alpha; // x: cutoff (0 = opaque), y: base alpha factor
};

void main(float4 position : SV_Position, float2 uv : TEXCOORD0)
{
    if (u_alpha.x > 0.0)
    {
        const float alpha = BaseColorTexture.Sample(BaseColorSampler, uv).a * u_alpha.y;
        clip(alpha - u_alpha.x);
    }
}
