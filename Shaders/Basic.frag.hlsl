// Fragment textures/samplers live in (t[n]/s[n], space2), uniforms in
// (b[n], space3) for SDL_GPU on D3D12.
Texture2D<float4> BaseColorTexture : register(t0, space2);
SamplerState BaseColorSampler : register(s0, space2);

cbuffer MaterialUniforms : register(b0, space3)
{
    float4 u_baseColorFactor;
    float4 u_emissiveFactor;
};

struct PSInput
{
    float4 position    : SV_Position;
    float3 worldNormal : TEXCOORD0;
    float2 uv          : TEXCOORD1;
};

float4 main(PSInput input) : SV_Target0
{
    const float3 normal = normalize(input.worldNormal);
    const float4 baseColor =
        BaseColorTexture.Sample(BaseColorSampler, input.uv) * u_baseColorFactor;

    // Placeholder overcast lighting: hemispheric sky/ground plus a soft sun.
    const float3 sunDirection = normalize(float3(0.3, 0.8, 0.4));
    const float3 skyColor = float3(0.75, 0.77, 0.80);
    const float3 groundColor = float3(0.20, 0.19, 0.17);

    const float hemisphere = normal.y * 0.5 + 0.5;
    const float3 ambient = lerp(groundColor, skyColor, hemisphere);
    const float sun = saturate(dot(normal, sunDirection)) * 0.45;

    const float3 lit = baseColor.rgb * (ambient + sun);
    const float3 emitted = baseColor.rgb * u_emissiveFactor.rgb;

    return float4(lit + emitted, baseColor.a);
}
