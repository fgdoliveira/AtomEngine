// Vertex sway (M19): foliage, grass and hanging cloth move in the wind.
// Each vertex carries its sway weight in the vertex colour's alpha, stored
// inverted so that unbaked meshes (alpha 1) stay rigid: weight = 1 - alpha,
// 0 at a card's root, up to 1 at its free end. The offset is a lean with
// the wind plus a few sines of time and world position, so neighbouring
// cards don't move in lockstep. Nothing is simulated.

// Per frame, vertex stage (b1, space1).
cbuffer WindUniforms : register(b1, space1)
{
    float4 u_wind; // xyz: wind velocity (m/s), w: time (s)
};

float3 ApplySway(float3 worldPosition, float swayWeight)
{
    if (swayWeight <= 0.0)
    {
        return worldPosition;
    }
    const float strength = length(u_wind.xz);
    const float2 along = strength > 0.0001 ? u_wind.xz / strength : float2(0.0, 0.0);
    const float phase = dot(worldPosition.xz, float2(0.61, 0.37));
    const float time = u_wind.w;
    const float wave = sin(time * 1.9 + phase) * 0.6 + sin(time * 3.3 + phase * 1.7) * 0.3;

    // Tips move most: a quadratic falloff keeps roots planted.
    const float w = swayWeight * swayWeight;
    const float2 lean = u_wind.xz * 0.10 + along * wave * 0.05 * (0.3 + strength);
    worldPosition.xz += lean * w;
    worldPosition.y -= 0.02 * w * abs(wave) * strength;
    return worldPosition;
}
