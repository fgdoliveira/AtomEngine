// Glow, step 1: keep what is brighter than the threshold, at 1/4 size.
// Four bilinear taps average a 4x4 block of scene pixels, so small bright
// details (a lamp, a sign's letters) aren't lost by the downsample.
Texture2D<float4> SceneTexture : register(t0, space2);
SamplerState SceneSampler : register(s0, space2);

cbuffer GlowBright : register(b0, space3)
{
    float4 u_bright; // x: threshold, yz: one scene texel
};

struct PSInput
{
    float4 position : SV_Position;
    float2 uv       : TEXCOORD0;
};

float4 main(PSInput input) : SV_Target0
{
    const float2 texel = u_bright.yz;
    float3 color = 0.0;
    color += SceneTexture.Sample(SceneSampler, input.uv + texel * float2(-1.0, -1.0)).rgb;
    color += SceneTexture.Sample(SceneSampler, input.uv + texel * float2( 1.0, -1.0)).rgb;
    color += SceneTexture.Sample(SceneSampler, input.uv + texel * float2(-1.0,  1.0)).rgb;
    color += SceneTexture.Sample(SceneSampler, input.uv + texel * float2( 1.0,  1.0)).rgb;
    color *= 0.25;

    // Soft knee: brightness above the threshold passes, easing in over a
    // band below it so glowing edges don't pop on and off.
    const float threshold = u_bright.x;
    const float brightness = max(color.r, max(color.g, color.b));
    const float knee = threshold * 0.5;
    const float soft = clamp(brightness - threshold + knee, 0.0, 2.0 * knee);
    const float contribution = max(soft * soft / (4.0 * knee + 1e-4), brightness - threshold);
    return float4(color * (contribution / max(brightness, 1e-4)), 1.0);
}
