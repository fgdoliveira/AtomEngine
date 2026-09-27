struct PSInput
{
    float4 position    : SV_Position;
    float3 worldNormal : TEXCOORD0;
    float3 color       : TEXCOORD1;
};

float4 main(PSInput input) : SV_Target0
{
    const float3 normal = normalize(input.worldNormal);

    // Placeholder overcast lighting: hemispheric sky/ground plus a soft sun.
    const float3 sunDirection = normalize(float3(0.3, 0.8, 0.4));
    const float3 skyColor = float3(0.75, 0.77, 0.80);
    const float3 groundColor = float3(0.20, 0.19, 0.17);

    const float hemisphere = normal.y * 0.5 + 0.5;
    const float3 ambient = lerp(groundColor, skyColor, hemisphere);
    const float sun = saturate(dot(normal, sunDirection)) * 0.45;

    return float4(input.color * (ambient + sun), 1.0);
}
