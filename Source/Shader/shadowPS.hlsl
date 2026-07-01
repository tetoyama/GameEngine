#include "common.hlsl"
#include "commonDefine.h"

Texture2D g_Texture : register(t0);
SamplerState g_SamplerState : register(s0);

float ResolvePerspectiveFarPlane()
{
    // DirectX LH perspective matrix (row-vector convention):
    // A = far / (far - near)
    // B = -near * far / (far - near)
    // far = -B / (A - 1)
    const float a = Projection[2][2];
    const float b = Projection[3][2];
    const float denominator = a - 1.0f;

    if (abs(denominator) <= 0.000001f)
    {
        return LOCAL_LIGHT_SHADOW_NEAR_PLANE +
            LOCAL_LIGHT_SHADOW_MIN_DEPTH_SPAN;
    }

    return max(
        abs(b / denominator),
        LOCAL_LIGHT_SHADOW_NEAR_PLANE +
            LOCAL_LIGHT_SHADOW_MIN_DEPTH_SPAN);
}

float main(in PS_IN In) : SV_Depth
{
    float4 outDiffuse = Material.BaseColor;
    if ((Material.MaterialFlags & MATERIAL_FLAG_USE_DIFFUSE_TEXTURE) != 0)
    {
        outDiffuse *= g_Texture.Sample(g_SamplerState, In.TexCoord);
    }
    if (outDiffuse.a <= ALPHA_CLIP_THRESHOLD)
    {
        discard;
    }

    // Perspective Point / Spot shadows store light-relative radial depth.
    // This depth is independent of the selected cubemap face, so crossing a
    // major-axis boundary cannot change the comparison space.
    const bool perspectiveProjection = abs(Projection[3][3]) < 0.5f;
    if (perspectiveProjection)
    {
        const float farPlane = ResolvePerspectiveFarPlane();
        const float radialDistance =
            length(In.WorldPosition.xyz - CameraPosition.xyz);
        return saturate(radialDistance / farPlane);
    }

    // Directional / CSM retain the hardware projected depth contract.
    return saturate(In.Position.z);
}
