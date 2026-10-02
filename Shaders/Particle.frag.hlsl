#include "Common.hlsli"

Texture2D<float4> AtlasTexture : register(t0, space2);
SamplerState AtlasSampler : register(s0, space2);

struct PSInput
{
    float4 position      : SV_Position;
    float2 uv            : TEXCOORD0;
    float4 color         : TEXCOORD1;
    float3 worldPosition : TEXCOORD2;
    float  beamLit       : TEXCOORD3;
};

float4 main(PSInput input) : SV_Target0
{
    const float4 texel = AtlasTexture.Sample(AtlasSampler, input.uv);
    float4 color = texel * input.color;

    // Fade out right in front of the camera so a leaf never fills the view.
    const float nearDistance = length(input.worldPosition - u_cameraPosition.xyz);
    color.a *= saturate((nearDistance - 0.4) / 1.0);

    // Dust (M45): invisible in the dark, lit where the spot reaches - the
    // beam made visible by what floats in it.
    const float beam = saturate(SpotReach(input.worldPosition) * 3.0);
    color.rgb = lerp(color.rgb, color.rgb * u_spotColor.rgb * beam, input.beamLit);
    color.a *= lerp(1.0, beam, input.beamLit);

    // Same fog as the scene: particles in the distance melt into it too.
    color.rgb = lerp(color.rgb, u_fogColor.rgb, ComputeFog(input.worldPosition));
    return color;
}
