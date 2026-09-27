// Final pass: HDR scene target -> swapchain (which encodes sRGB).
// Order: exposure -> tonemap -> grade -> grain -> vignette.
Texture2D<float4> SceneTexture : register(t0, space2);
SamplerState SceneSampler : register(s0, space2);

// Mirrors PostUniforms in Renderer.cpp.
cbuffer PostUniforms : register(b0, space3)
{
    float4 u_tint;       // rgb tint, w: enabled
    float4 u_params;     // x: exposure, y: saturation, z: grain, w: vignette
    float4 u_output;     // xy: output size in pixels, z: frame index
};

struct PSInput
{
    float4 position : SV_Position;
    float2 uv       : TEXCOORD0;
};

// Narkowicz's fit of the ACES filmic curve: rolls highlights off smoothly
// instead of clipping, and adds gentle contrast in the mid-tones.
float3 TonemapAces(float3 x)
{
    return saturate((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14));
}

float Luminance(float3 color)
{
    return dot(color, float3(0.2126, 0.7152, 0.0722));
}

// Cheap per-pixel hash; the frame index makes the grain move each frame.
float Hash(float2 pixel, float frame)
{
    float3 p = frac(float3(pixel, frame) * float3(0.1031, 0.1030, 0.0973));
    p += dot(p, p.yzx + 33.33);
    return frac((p.x + p.y) * p.z);
}

float4 main(PSInput input) : SV_Target0
{
    float3 color = SceneTexture.Sample(SceneSampler, input.uv).rgb;

    if (u_tint.w <= 0.0)
    {
        return float4(color, 1.0);
    }

    color = TonemapAces(color * u_params.x);

    // Grade: pull toward grey, then a slight colour cast.
    color = lerp(Luminance(color).xxx, color, u_params.y) * u_tint.rgb;

    // Film grain, strongest in the mid-tones where it reads as texture
    // rather than as noise in blacks or highlights.
    if (u_params.z > 0.0)
    {
        const float noise = Hash(input.position.xy, u_output.z) - 0.5;
        const float luminance = Luminance(color);
        const float midtones = saturate(4.0 * luminance * (1.0 - luminance));
        color *= 1.0 + noise * 2.0 * u_params.z * (0.35 + 0.65 * midtones);
    }

    // Vignette, round regardless of the window's aspect ratio.
    if (u_params.w > 0.0)
    {
        const float aspect = u_output.x / u_output.y;
        const float2 offset = (input.uv - 0.5) * float2(aspect, 1.0);
        const float falloff = smoothstep(0.35, 1.05, length(offset));
        color *= 1.0 - u_params.w * falloff;
    }

    return float4(saturate(color), 1.0);
}
