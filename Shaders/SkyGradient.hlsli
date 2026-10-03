#ifndef ATOM_SKY_GRADIENT_HLSLI
#define ATOM_SKY_GRADIENT_HLSLI

// Day sky (M47): authored colours, not a simulated atmosphere. The horizon
// colour holds a wide band low down and the zenith colour arrives higher
// up; below the horizon stays the horizon colour, which the fog matches, so
// the ground melts into the sky. The sun is a hot disc (bright enough for
// the glow pass to bloom) inside a soft halo. Shared with the water (M48),
// which reflects it.
float3 SkyGradient(float3 direction, float3 zenith, float3 horizon,
                   float3 sunDirection, float3 sunColor, float sunCos, float sunGlow)
{
    const float height = saturate(direction.y);
    float3 color = lerp(horizon, zenith, smoothstep(0.0, 0.65, height));

    const float toSun = dot(direction, sunDirection);
    // A wide faint halo and a tighter bright one.
    const float halo = pow(saturate(toSun), 6.0) * 0.25 + pow(saturate(toSun), 64.0) * 0.6;
    color += sunColor * halo * sunGlow;
    // The disc, its edge softened over a quarter of its radius.
    const float disc = smoothstep(sunCos, lerp(sunCos, 1.0, 0.25), toSun);
    color += sunColor * disc * 12.0;
    return color;
}

#endif
