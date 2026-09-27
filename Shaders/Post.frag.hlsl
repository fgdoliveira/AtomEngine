// Final pass: scene target -> swapchain. The swapchain encodes sRGB.
// Pass-through for now; tonemapping and grading arrive with M7.4.
Texture2D<float4> SceneTexture : register(t0, space2);
SamplerState SceneSampler : register(s0, space2);

struct PSInput
{
    float4 position : SV_Position;
    float2 uv       : TEXCOORD0;
};

float4 main(PSInput input) : SV_Target0
{
    return float4(SceneTexture.Sample(SceneSampler, input.uv).rgb, 1.0);
}
