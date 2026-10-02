// Shared by the scene shaders. Mirrors SceneUniforms in Renderer.cpp.
#ifndef ATOM_COMMON_HLSLI
#define ATOM_COMMON_HLSLI

cbuffer SceneUniforms : register(b1, space3)
{
    float4x4 u_lightViewProjection;
    float4 u_sunDirection;   // xyz: towards the sun
    float4 u_sunColor;
    float4 u_skyColor;
    float4 u_groundColor;
    float4 u_fogColor;       // rgb, w: density
    float4 u_cameraPosition; // xyz, w: height falloff
    float4 u_fogParams;      // x: base height
    float4 u_shadowParams;   // x: enabled, y: texel size (uv), z: ambient share, w: normal offset (m)
    float4 u_time;           // x: seconds, y: live light count (M25)
    float4 u_liveLightPosition[4]; // xyz, w: radius
    float4 u_liveLightColor[4];    // rgb (times intensity)
    float4 u_spotPosition;         // M42: xyz, w: range
    float4 u_spotDirection;        // xyz, w: 1 on, 0 off
    float4 u_spotColor;            // rgb times intensity, w: specular scale
    float4 u_spotCone;             // x: cos outer, y: cos inner
    float4x4 u_spotViewProjection; // M43: its shadow map
    float4 u_spotShadow;           // x: on, y: texel size (uv), z: normal offset per metre
    float4 u_skyZenith;            // M48: rgb, w: 1 = gradient sky
    float4 u_skyHorizon;           // rgb, w: cosine of the sun disc's radius
    float4 u_skySun;               // x: halo strength, y: water glint (M49)
    float4 u_waterShallow;         // rgb, w: sky reflection
    float4 u_waterDeep;            // rgb, w: ripple
};

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

// How much of the spot light (M42) reaches a point, before the surface's
// angle: the cone times the windowed inverse-square falloff, 0 when off.
// Used by what the spot lights without a surface: dust, the beam (M45).
float SpotReach(float3 worldPosition)
{
    const float3 fromLamp = worldPosition - u_spotPosition.xyz;
    const float distance = length(fromLamp);
    const float cone = smoothstep(u_spotCone.x, u_spotCone.y,
                                  dot(fromLamp / max(distance, 1e-4), u_spotDirection.xyz));
    const float ratio = distance / u_spotPosition.w;
    const float window = saturate(1.0 - ratio * ratio * ratio * ratio);
    return cone * window * window / (distance * distance + 1.0) * u_spotDirection.w;
}

// Exponential height fog, integrated along the view ray: density falls off
// exponentially with height, so looking up clears faster than looking along
// the ground. Returns the fraction of fog colour to apply.
float ComputeFog(float3 worldPosition)
{
    const float density = u_fogColor.w;
    if (density <= 0.0)
    {
        return 0.0;
    }

    const float falloff = u_cameraPosition.w;
    const float3 ray = worldPosition - u_cameraPosition.xyz;
    const float distance = length(ray);

    const float cameraHeight = u_cameraPosition.y - u_fogParams.x;
    const float heightScale = exp(-falloff * cameraHeight);

    // Average density along the ray relative to the camera height.
    const float climb = falloff * ray.y;
    const float average = abs(climb) > 1e-4
        ? (1.0 - exp(-climb)) / climb
        : 1.0;

    const float opticalDepth = density * heightScale * average * distance;
    return 1.0 - exp(-opticalDepth);
}

#endif
