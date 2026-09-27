// Fragment textures/samplers live in (t[n]/s[n], space2), uniforms in
// (b[n], space3) for SDL_GPU on D3D12.
Texture2D<float4> BaseColorTexture : register(t0, space2);
SamplerState BaseColorSampler : register(s0, space2);

// Per draw.
cbuffer MaterialUniforms : register(b0, space3)
{
    float4 u_baseColorFactor;
    float4 u_emissiveFactor;
};

// Per frame. Mirrors SceneUniforms in Renderer.cpp.
cbuffer SceneUniforms : register(b1, space3)
{
    float4 u_sunDirection;   // xyz: towards the sun
    float4 u_sunColor;
    float4 u_skyColor;
    float4 u_groundColor;
    float4 u_fogColor;       // rgb, w: density
    float4 u_cameraPosition; // xyz, w: height falloff
    float4 u_fogParams;      // x: base height
};

struct PSInput
{
    float4 position      : SV_Position;
    float3 worldNormal   : TEXCOORD0;
    float2 uv            : TEXCOORD1;
    float3 worldPosition : TEXCOORD2;
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

float4 main(PSInput input) : SV_Target0
{
    const float3 normal = normalize(input.worldNormal);
    const float4 baseColor =
        BaseColorTexture.Sample(BaseColorSampler, input.uv) * u_baseColorFactor;

    // Overcast: hemispheric sky/ground ambient plus a soft Lambert sun.
    const float hemisphere = normal.y * 0.5 + 0.5;
    const float3 ambient = lerp(u_groundColor.rgb, u_skyColor.rgb, hemisphere);
    const float3 sun =
        saturate(dot(normal, u_sunDirection.xyz)) * u_sunColor.rgb;

    const float3 lit = baseColor.rgb * (ambient + sun);
    const float3 emitted = baseColor.rgb * u_emissiveFactor.rgb;

    const float fog = ComputeFog(input.worldPosition);
    const float3 color = lerp(lit + emitted, u_fogColor.rgb, fog);

    return float4(color, baseColor.a);
}
