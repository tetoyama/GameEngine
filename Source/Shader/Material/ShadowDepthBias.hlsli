#ifndef SHADOW_DEPTH_BIAS_HLSLI
#define SHADOW_DEPTH_BIAS_HLSLI

#ifndef COMMON_DEFINE_H
#include "../commonDefine.h"
#endif

float ResolveOrthographicShadowDepthBias(
    float legacyNdcBias,
    float receiverNdotL)
{
    const float baseNdcBias = max(legacyNdcBias, 0.0f);

    const float safeNdotL = saturate(receiverNdotL);
    const float grazing = 1.0f - safeNdotL;
    const float slopeScale = 1.0f + grazing * grazing * 8.0f;

    return min(
        baseNdcBias * slopeScale,
        LOCAL_LIGHT_SHADOW_MAX_NDC_BIAS);
}

float ResolvePerspectiveShadowDepthBias(
    float viewDepth,
    float farPlane,
    float projectedDepthBias,
    float receiverNdotL)
{
    // Existing scenes store Param.w as projected-depth/NDC bias. Converting
    // that value as if it were a world-space distance reduced 0.005 to roughly
    // 1e-6 for ordinary Point-Light distances and caused severe face-dependent
    // self-shadowing. Keep the persisted contract independent of range/depth.
    return min(
        max(projectedDepthBias, 0.0f),
        LOCAL_LIGHT_SHADOW_MAX_NDC_BIAS);
}

float ResolvePerspectiveShadowDepthBias(
    float viewDepth,
    float farPlane,
    float projectedDepthBias)
{
    return ResolvePerspectiveShadowDepthBias(
        viewDepth,
        farPlane,
        projectedDepthBias,
        1.0f);
}

#endif // SHADOW_DEPTH_BIAS_HLSLI
