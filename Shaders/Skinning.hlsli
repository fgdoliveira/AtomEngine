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

#endif
