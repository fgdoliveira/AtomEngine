// The flashlight's fake beam (M45): light seen in the air, faked. Each
// plane glows by the spot's own reach at that point (cone x falloff, the
// same maths that lights the walls), so the beam is brightest near the
// lamp along its axis and fades out at the cone's edge and with distance.
// Added onto the scene; walls in front hide it through the depth test.
#include "Common.hlsli"

cbuffer BeamUniforms : register(b0, space3)
{
    float4 u_beam; // x: strength (haze in the air), y: fade-in distance from the eye (m)
};

struct PSInput
{
    float4 position      : SV_Position;
    float3 worldPosition : TEXCOORD0;
    float3 worldNormal   : TEXCOORD1;
};

float4 main(PSInput input) : SV_Target0
{
    const float3 toEye = u_cameraPosition.xyz - input.worldPosition;
    const float eyeDistance = length(toEye);
    // A plane seen edge-on would draw a hard line: fade it out as it turns
    // away, and keep the part right at the lens from filling the view.
    const float facing = abs(dot(normalize(input.worldNormal), toEye / max(eyeDistance, 1e-4)));
    const float nearFade = saturate((eyeDistance - u_beam.y) / max(u_beam.y, 1e-3));
    const float glow = SpotReach(input.worldPosition) * facing * nearFade * u_beam.x;
    const float fogged = 1.0 - ComputeFog(input.worldPosition);
    return float4(u_spotColor.rgb * glow * fogged, 1.0);
}
