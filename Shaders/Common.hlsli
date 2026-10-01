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
};

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
