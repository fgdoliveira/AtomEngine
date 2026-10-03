// Stylized water (M48): early-2000s fantasy water on a modern GPU. It reads
// through colour, movement and highlights rather than optics - no scene
// reflection, no refraction:
//
//   body colour (shallow -> deep)  lit by sky and sun
//   + sky colour, by Fresnel       along the rippled reflection
//   + the sun's glint              on the ripples
//   + foam                         where it meets the shore
//
// The mesh carries how deep the water is in its first UV's u (0 at the
// shore, 1 deep, baked by Blender); the surface pattern comes from world
// position, so neighbouring water meshes ripple as one.
#include "Common.hlsli"
#include "SkyGradient.hlsli"

// The scene's bindings (the water pipeline shares its layout); water reads
// only the sun's shadow map.
Texture2D<float4> BaseColorTexture : register(t0, space2);
SamplerState BaseColorSampler : register(s0, space2);
Texture2D<float> ShadowMap : register(t1, space2);
SamplerComparisonState ShadowSampler : register(s1, space2);
Texture2D<float4> Lightmap : register(t2, space2);
SamplerState LightmapSampler : register(s2, space2);
Texture2D<float4> EmissiveTexture : register(t3, space2);
SamplerState EmissiveSampler : register(s3, space2);
Texture2D<float> SpotShadowMap : register(t4, space2);
SamplerComparisonState SpotShadowSampler : register(s4, space2);
// M51: the scene mirrored in the water's plane (half
// resolution, the image flipped left-right), when u_weather.y says so.
Texture2D<float4> ReflectionTexture : register(t5, space2);
SamplerState ReflectionSampler : register(s5, space2);

cbuffer MaterialUniforms : register(b0, space3)
{
    float4 u_baseColorFactor;
    float4 u_emissiveFactor;
    float4 u_lightmap;
    float4 u_alpha;          // w: fog amount
    float4 u_surface;
    float4 u_lights;
};

struct PSInput
{
    float4 position      : SV_Position;
    float3 worldNormal   : TEXCOORD0;
    float2 uv            : TEXCOORD1; // u: depth, 0 at the shore .. 1 deep
    float3 worldPosition : TEXCOORD2;
    float4 color         : TEXCOORD3;
    float2 lightmapUv    : TEXCOORD4;
};

// One tap of the sun's shadow map: soft edges matter little on a moving
// surface, and the jetty's shadow still lies on the water.
float SunShadow(float3 worldPosition)
{
    if (u_shadowParams.x <= 0.0)
    {
        return 1.0;
    }
    const float4 light = mul(u_lightViewProjection, float4(worldPosition, 1.0));
    const float3 ndc = light.xyz / light.w;
    const float2 uv = float2(ndc.x * 0.5 + 0.5, 0.5 - ndc.y * 0.5);
    if (any(uv < 0.0) || any(uv > 1.0) || ndc.z > 1.0)
    {
        return 1.0;
    }
    return ShadowMap.SampleCmpLevelZero(ShadowSampler, uv, ndc.z);
}

// The surface's height: two layers of noise drifting different ways (the
// big slow swell and smaller wind ripples), plus a fine layer that makes
// the glint sparkle. Only its slopes are used.
float WaterHeight(float2 p, float t)
{
    const float swell = ValueNoise(p * 0.35 + float2(t * 0.11, t * 0.05));
    const float ripples = ValueNoise(float2(p.x * 0.6 + p.y * 0.55, p.y * 0.6 - p.x * 0.55) * 1.4
                                     + float2(-t * 0.25, t * 0.18));
    const float sparkle = ValueNoise(p * 3.1 + float2(t * 0.6, -t * 0.45));
    return swell * 0.6 + ripples * 0.3 + sparkle * 0.1;
}

// Rain on the water (M50): rings spreading from where drops land. A grid
// of cells, each with a drop at a jittered spot on its own clock; a ring
// grows and fades over its life. Returns a slope to add to the normal.
float2 RainRings(float2 p, float t, float rain)
{
    float2 slope = 0.0;
    [unroll] for (int layer = 0; layer < 2; ++layer)
    {
        const float scale = layer == 0 ? 1.1 : 0.8;
        const float2 q = p / scale + layer * 17.3;
        const float2 cell = floor(q);
        const float2 jitter = float2(Hash(cell), Hash(cell + 31.7)) * 0.6 + 0.2;
        const float2 local = frac(q) - jitter;
        const float age = frac(t * (0.7 + 0.4 * Hash(cell + 5.1)) + Hash(cell + 9.3));
        // Heavier rain: more cells have a drop.
        const float active = step(Hash(cell + 2.9 + floor(t * 0.7 + Hash(cell + 9.3))), rain);
        const float d = length(local) * scale;
        const float radius = age * 0.45;
        const float ring = exp(-pow((d - radius) * 22.0, 2.0)) * (1.0 - age) * active;
        slope += (local / max(length(local), 1e-3)) * ring;
    }
    return slope * 0.9;
}

// The normal from the height's slopes (central differences).
float3 WaterNormal(float2 p, float t, float strength)
{
    const float e = 0.12;
    const float dx = WaterHeight(p + float2(e, 0.0), t) - WaterHeight(p - float2(e, 0.0), t);
    const float dz = WaterHeight(p + float2(0.0, e), t) - WaterHeight(p - float2(0.0, e), t);
    const float slope = strength * 0.9 / (2.0 * e);
    return normalize(float3(-dx * slope, 1.0, -dz * slope));
}

float4 main(PSInput input) : SV_Target0
{
    const float depth = saturate(input.uv.x);
    const float3 position = input.worldPosition;
    const float t = u_time.x;

    float3 n = WaterNormal(position.xz, t, u_waterDeep.w);
    [branch] if (u_weather.x > 0.0)
    {
        const float2 rings = RainRings(position.xz, t, u_weather.x);
        n = normalize(n + float3(rings.x, 0.0, rings.y));
    }
    const float3 toEye = normalize(u_cameraPosition.xyz - position);
    const float3 toSun = normalize(u_sunDirection.xyz);
    const float shadow = SunShadow(position + float3(0.0, 0.05, 0.0));

    // The body: authored tints, lit by the sky and (less) by the sun.
    const float3 tint = lerp(u_waterShallow.rgb, u_waterDeep.rgb, smoothstep(0.0, 1.0, depth));
    const float3 light = u_skyColor.rgb * 0.75 + u_sunColor.rgb * saturate(dot(n, toSun)) * shadow * 0.6;
    const float3 body = tint * light;

    // The sky along the rippled reflection, kept above the horizon. Bright
    // sun pixels are capped: the glint below draws the sun's highlight.
    float3 reflected = reflect(-toEye, n);
    reflected.y = abs(reflected.y);
    float3 sky;
    [branch] if (u_skyZenith.w > 0.5)
    {
        sky = SkyGradient(reflected, u_skyZenith.rgb, u_skyHorizon.rgb,
                          toSun, u_sunColor.rgb, u_skyHorizon.w, u_skySun.x);
        sky = min(sky, 2.0);
    }
    else
    {
        sky = lerp(u_fogColor.rgb, u_skyColor.rgb, saturate(reflected.y * 2.0));
    }

    // The planar reflection (M51): where this pixel is on
    // screen, mirrored left-right like the reflection image, nudged by the
    // ripples so the reflection wobbles.
    [branch] if (u_weather.y > 0.5)
    {
        const float2 screen = input.position.xy * u_weather.zw;
        const float2 uv = float2(1.0 - screen.x, screen.y) + n.xz * 0.05;
        sky = min(ReflectionTexture.Sample(ReflectionSampler, uv).rgb, 2.0);
    }

    // Fresnel (Schlick): water reflects ~2 % looking straight down and
    // nearly everything at a grazing angle.
    const float facing = saturate(dot(n, toEye));
    const float fresnel = (0.02 + 0.98 * pow(1.0 - facing, 5.0)) * u_waterShallow.w;
    float3 color = lerp(body, sky, fresnel);

    // The sun's glint: a tight hot highlight and a broad sheen, broken up
    // by the ripples (Blinn-Phong on the rippled normal).
    const float3 halfway = normalize(toSun + toEye);
    const float nh = saturate(dot(n, halfway));
    color += u_sunColor.rgb * shadow * u_skySun.y * (pow(nh, 700.0) * 6.0 + pow(nh, 90.0) * 0.25);

    // Foam: a band along the shore, broken and drifting.
    const float band = saturate(1.0 - depth / 0.14);
    const float broken = ValueNoise(position.xz * 1.8 + float2(t * 0.3, -t * 0.2));
    const float foam = smoothstep(0.45, 0.85, band * (0.55 + 0.6 * broken));
    const float3 foamColor = (u_skyColor.rgb * 0.85 + u_sunColor.rgb * saturate(toSun.y) * shadow) * 0.9;
    color = lerp(color, foamColor, foam);

    // Clear at the edge, nearly opaque in the deep; what it reflects and
    // its foam are opaque. Zero at the waterline, so the edge is soft.
    float alpha = lerp(0.35, 0.94, smoothstep(0.0, 0.6, depth));
    alpha = max(max(alpha, fresnel), foam) * smoothstep(0.0, 0.02, depth);

    const float fog = ComputeFog(position) * u_alpha.w;
    color = lerp(color, u_fogColor.rgb, fog);
    return float4(color, alpha);
}
