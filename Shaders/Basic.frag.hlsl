// Fragment textures/samplers live in (t[n]/s[n], space2), uniforms in
// (b[n], space3) for SDL_GPU on D3D12.
#include "Common.hlsli"

Texture2D<float4> BaseColorTexture : register(t0, space2);
SamplerState BaseColorSampler : register(s0, space2);

Texture2D<float> ShadowMap : register(t1, space2);
SamplerComparisonState ShadowSampler : register(s1, space2);

Texture2D<float4> Lightmap : register(t2, space2);
SamplerState LightmapSampler : register(s2, space2);

// The spot's shadow map (M43), compared like the sun's.
Texture2D<float> SpotShadowMap : register(t4, space2);
SamplerComparisonState SpotShadowSampler : register(s4, space2);

// Which pixels glow (M23); used when u_alpha.z says the material has one.
Texture2D<float4> EmissiveTexture : register(t3, space2);
SamplerState EmissiveSampler : register(s3, space2);

// Per draw.
cbuffer MaterialUniforms : register(b0, space3)
{
    float4 u_baseColorFactor;
    float4 u_emissiveFactor; // w: baked-light weight (0 = none)
    float4 u_lightmap;       // x: intensity, y: weight (0 = no lightmap),
                             // z: wet (M25), w: debug - show the vertex
                             // colour as the base colour (skin weights, M36)
    float4 u_alpha;          // x: cutoff (0 = opaque), y: alpha-to-coverage,
                             // z: has emissive texture, w: fog amount
    float4 u_surface;        // M42: x shininess (Blinn-Phong), y specular strength,
                             // z revealed by the spot (M44)
};

struct PSInput
{
    float4 position      : SV_Position;
    float3 worldNormal   : TEXCOORD0;
    float2 uv            : TEXCOORD1;
    float3 worldPosition : TEXCOORD2;
    float4 color         : TEXCOORD3; // baked light, linear
    float2 lightmapUv    : TEXCOORD4;
};

// 1 = fully lit, 0 = fully shadowed. The comparison sampler already blends
// 2x2 texels; a 3x3 grid of those gives soft, overcast-friendly edges.
float ComputeShadow(float3 worldPosition, float3 normal)
{
    if (u_shadowParams.x <= 0.0)
    {
        return 1.0;
    }

    // Push the lookup off the surface along its normal to avoid acne.
    const float3 offsetPosition = worldPosition + normal * u_shadowParams.w;
    const float4 lightPosition =
        mul(u_lightViewProjection, float4(offsetPosition, 1.0));
    const float3 ndc = lightPosition.xyz / lightPosition.w;

    // Clip space Y is up, texture V is down.
    const float2 uv = float2(ndc.x * 0.5 + 0.5, 0.5 - ndc.y * 0.5);
    if (any(uv < 0.0) || any(uv > 1.0) || ndc.z > 1.0)
    {
        return 1.0; // outside the shadow box: lit
    }

    const float texel = u_shadowParams.y;
    float visibility = 0.0;
    [unroll] for (int y = -1; y <= 1; ++y)
    {
        [unroll] for (int x = -1; x <= 1; ++x)
        {
            visibility += ShadowMap.SampleCmpLevelZero(
                ShadowSampler, uv + float2(x, y) * texel, ndc.z);
        }
    }
    return visibility / 9.0;
}

// Value noise in [0, 1] (M25): a hash per lattice point, smoothly blended.
float Hash(float2 p)
{
    return frac(sin(dot(p, float2(127.1, 311.7))) * 43758.5453);
}

float ValueNoise(float2 p)
{
    const float2 i = floor(p);
    const float2 f = frac(p);
    const float2 s = f * f * (3.0 - 2.0 * f);
    return lerp(lerp(Hash(i), Hash(i + float2(1, 0)), s.x),
                lerp(Hash(i + float2(0, 1)), Hash(i + float2(1, 1)), s.x), s.y);
}

// Live point lights (M25): Lambert with a falloff that reaches zero at the
// radius, so a light's reach is exact and cheap to reason about.
float3 LiveLights(float3 worldPosition, float3 normal)
{
    float3 light = 0.0;
    const int count = (int)u_time.y;
    [loop] for (int i = 0; i < count; ++i)
    {
        const float3 toLight = u_liveLightPosition[i].xyz - worldPosition;
        const float distance = length(toLight);
        const float reach = saturate(1.0 - distance / u_liveLightPosition[i].w);
        const float lambert = saturate(dot(normal, toLight / max(distance, 1e-4)));
        light += u_liveLightColor[i].rgb * (reach * reach * lambert);
    }
    return light;
}

// 1 = lit by the spot, 0 = something stands between it and the lamp. The
// same idea as the sun's, from a perspective view: a texel covers more of
// the world further from the lamp, so the normal offset grows with the
// distance. Points outside its frustum are lit (the cone already decides).
float ComputeSpotShadow(float3 worldPosition, float3 normal, float distance)
{
    const float3 offsetPosition = worldPosition + normal * (u_spotShadow.z * max(distance, 0.5));
    const float4 lightPosition = mul(u_spotViewProjection, float4(offsetPosition, 1.0));
    const float3 ndc = lightPosition.xyz / max(lightPosition.w, 1e-4);
    const float2 uv = float2(ndc.x * 0.5 + 0.5, 0.5 - ndc.y * 0.5);

    const float texel = u_spotShadow.y;
    float visibility = 0.0;
    [unroll] for (int y = -1; y <= 1; ++y)
    {
        [unroll] for (int x = -1; x <= 1; ++x)
        {
            visibility += SpotShadowMap.SampleCmpLevelZero(SpotShadowSampler, uv + float2(x, y) * texel, ndc.z);
        }
    }
    // Outside the map (behind the lamp, beyond the range): lit.
    const bool inside = all(uv >= 0.0) && all(uv <= 1.0) && ndc.z <= 1.0 && lightPosition.w > 0.0;
    return inside ? visibility / 9.0 : 1.0;
}

// The spot light (M42). Mirrors SpotMath::Evaluate (Engine/Renderer/SpotLight.h).
// x: diffuse (times base colour), y: specular (times light colour). All
// arithmetic, no branches: when the spot is off its colour is zero.
float2 SpotLighting(float3 worldPosition, float3 normal)
{
    const float3 toLight = u_spotPosition.xyz - worldPosition;
    const float distance = length(toLight);
    const float3 l = toLight / max(distance, 1e-4);

    // The cone: full inside the inner angle, nothing past the outer one.
    const float cone = smoothstep(u_spotCone.x, u_spotCone.y, dot(-l, u_spotDirection.xyz));
    // Inverse-square, +1 at the lamp, windowed to exactly 0 at the range.
    const float ratio = distance / u_spotPosition.w;
    const float window = saturate(1.0 - ratio * ratio * ratio * ratio);
    float reach = cone * window * window / (distance * distance + 1.0) * u_spotDirection.w;
    // The 9 shadow taps only where the spot reaches at all: with the spot
    // off (or outside its cone - most of the screen) they cost nothing.
    // Like the sun's early return, the branch follows whole regions of the
    // screen, not a coin-flip per pixel.
    [branch] if (u_spotShadow.x > 0.0 && reach > 0.0)
    {
        reach *= ComputeSpotShadow(worldPosition, normal, distance);
    }

    const float lambert = saturate(dot(normal, l));
    // Blinn-Phong: the normal against the half vector of light and eye.
    const float3 h = normalize(l + normalize(u_cameraPosition.xyz - worldPosition));
    const float shininess = u_surface.x;
    const float spec = pow(saturate(dot(normal, h)), shininess) * (shininess + 8.0) / 8.0
        * u_surface.y * u_spotColor.w * (lambert > 0.0 ? 1.0 : 0.0);
    return float2(reach * lambert, reach * spec);
}

float4 main(PSInput input, bool frontFace : SV_IsFrontFace) : SV_Target0
{
    // Double-sided cards are lit from whichever side we see.
    const float3 normal = normalize(input.worldNormal) * (frontFace ? 1.0 : -1.0);
    float4 baseColor =
        BaseColorTexture.Sample(BaseColorSampler, input.uv) * u_baseColorFactor;
    // Debug views (M36): the vertex colour as the base colour (skin
    // weights). Blended, not branched: a branch here cost ~10% of the frame
    // on Iris Xe in lightmapped scenes.
    baseColor = lerp(baseColor, float4(input.color.rgb, 1.0), u_lightmap.w);

    // Alpha testing (M17). With MSAA, alpha-to-coverage: the alpha is
    // sharpened to a ~1-pixel ramp around the cutoff, and the hardware turns
    // it into covered samples - antialiased edges without sorting.
    if (u_alpha.x > 0.0)
    {
        if (u_alpha.y > 0.0)
        {
            baseColor.a = saturate((baseColor.a - u_alpha.x)
                / max(fwidth(baseColor.a), 0.0001) + 0.5);
        }
        else
        {
            clip(baseColor.a - u_alpha.x);
            baseColor.a = 1.0;
        }
    }

    const float shadow = ComputeShadow(input.worldPosition, normal);

    // Overcast: hemispheric sky/ground ambient plus a soft Lambert sun.
    // Shadows remove the sun and a share of the sky light, so they stay
    // visible under weak, diffuse sunlight.
    const float hemisphere = normal.y * 0.5 + 0.5;
    const float3 hemisphereAmbient =
        lerp(u_groundColor.rgb, u_skyColor.rgb, hemisphere);
    // Baked (M15): the vertex colour is how much of a white sky, plus its
    // bounces, reaches this point, so it already holds the orientation and
    // the occlusion the hemisphere term only approximates.
    const float3 bakedAmbient = u_skyColor.rgb * input.color.rgb;
    float3 ambient = lerp(hemisphereAmbient, bakedAmbient, u_emissiveFactor.w);
    // Lightmap (M16): baked direct + bounced light of the level's own
    // lights, resolved across each face. Replaces the ambient estimate.
    const float3 lightmap =
        Lightmap.Sample(LightmapSampler, input.lightmapUv).rgb * u_lightmap.x;
    ambient = lerp(ambient, lightmap, u_lightmap.y)
        * lerp(1.0, shadow, u_shadowParams.z);
    // Thin alpha-tested cards (leaves, cloth) let light through: lit from
    // behind they glow at half strength instead of turning black.
    const float facing = dot(normal, u_sunDirection.xyz);
    const float sunLight = u_alpha.x > 0.0
        ? max(saturate(facing), 0.5 * saturate(-facing))
        : saturate(facing);
    const float3 sun = sunLight * u_sunColor.rgb * shadow;

    const float2 spot = SpotLighting(input.worldPosition, normal);
    const float3 lit = baseColor.rgb * (ambient + sun + LiveLights(input.worldPosition, normal)
                                        + u_spotColor.rgb * spot.x)
                     + u_spotColor.rgb * spot.y;
    // An emissive mask says exactly what glows; without one, the base
    // colour does (older kit pieces: vending screens, shoji).
    const float3 emissiveSource = u_alpha.z > 0.0
        ? EmissiveTexture.Sample(EmissiveSampler, input.uv).rgb
        : baseColor.rgb;
    float3 emitted = emissiveSource * u_emissiveFactor.rgb;

    // Wet surfaces (M25): reflections break up in slow ripples, stretched
    // along the street, and a faint sheen drifts over the surface. Noise in
    // world space, so neighbouring decals ripple as one sheet of water.
    float3 sheen = 0.0;
    if (u_lightmap.z > 0.0)
    {
        const float2 p = input.worldPosition.xz * float2(0.9, 3.5);
        const float t = u_time.x;
        const float ripple = ValueNoise(p + float2(t * 0.35, t * 0.9))
                           * 0.6 + ValueNoise(p * 2.3 - float2(t * 0.7, 0.0)) * 0.4;
        emitted *= lerp(1.0, 0.35 + 1.3 * ripple, u_lightmap.z);
        // Brighter where the surface is lit: wet ground glints in lamp pools.
        sheen = (lit * 0.5 + u_skyColor.rgb * 0.6) * u_lightmap.z * smoothstep(0.62, 0.9, ripple);
    }

    const float fog = ComputeFog(input.worldPosition) * u_alpha.w;
    const float3 color = lerp(lit + emitted + sheen, u_fogColor.rgb, fog);

    // Revealed by light (M44): the decal's alpha follows the beam, so it's
    // there only where the flashlight shines (diffuse reach, saturating
    // well inside the beam).
    const float revealed = lerp(1.0, saturate(spot.x * u_spotColor.g * 0.5), u_surface.z);
    return float4(color, baseColor.a * revealed);
}
