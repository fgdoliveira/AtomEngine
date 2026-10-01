// Linear blend skinning (M35): a vertex follows up to four joints, each
// through its palette matrix (joint world * inverse bind), blended by
// weight. Vertex uniform slot 2 holds the palette.
#ifndef ATOM_SKINNING_HLSLI
#define ATOM_SKINNING_HLSLI

cbuffer JointPalette : register(b2, space1)
{
    float4x4 u_joints[64];
};

float4x4 SkinMatrix(uint4 joints, float4 weights)
{
    return u_joints[joints.x] * weights.x
         + u_joints[joints.y] * weights.y
         + u_joints[joints.z] * weights.z
         + u_joints[joints.w] * weights.w;
}

// A distinct colour per joint (golden-ratio hues), blended like the
// positions are: smooth gradients where joints share vertices.
float3 JointColor(uint joint)
{
    const float hue = frac(joint * 0.618034 + 0.12);
    const float3 rgb = saturate(abs(frac(hue + float3(0.0, 2.0 / 3.0, 1.0 / 3.0)) * 6.0 - 3.0) - 1.0);
    return lerp(float3(0.9, 0.9, 0.9), rgb, 0.85);
}

float3 WeightColor(uint4 joints, float4 weights)
{
    return JointColor(joints.x) * weights.x + JointColor(joints.y) * weights.y
         + JointColor(joints.z) * weights.z + JointColor(joints.w) * weights.w;
}

#endif
