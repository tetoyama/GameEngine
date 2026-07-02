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

    // Directional / CSM use orthographic light projection, so Param.w is
    // already an NDC-depth bias. Increase only that existing bias when the
    // receiver is nearly parallel to the light direction.
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
    float worldBias,
    float receiverNdotL)
{
    const float nearPlane = LOCAL_LIGHT_SHADOW_NEAR_PLANE;
    const float safeFar = max(
        farPlane,
        nearPlane + LOCAL_LIGHT_SHADOW_MIN_DEPTH_SPAN);
    const float safeDepth = clamp(abs(viewDepth), nearPlane, safeFar);

    // Point cubemap side faces observe horizontal receivers at a grazing
    // angle. A distance-only bias is then insufficient and causes the hard
    // major-axis face boundary to appear as a square self-shadow region.
    const float safeNdotL = saturate(receiverNdotL);
    const float grazing = 1.0f - safeNdotL;
    const float slopeScale = 1.0f + grazing * grazing * 8.0f;
    const float effectiveWorldBias = max(worldBias, 0.0f) * slopeScale;

    // DirectX LH perspective depth:
    // z_ndc = far/(far-near) - near*far/((far-near)*viewZ)
    // Convert the slope-adjusted world-distance bias to NDC at this receiver.
    const float depthDerivative =
        (nearPlane * safeFar) /
        ((safeFar - nearPlane) * safeDepth * safeDepth);

    return min(
        effectiveWorldBias * depthDerivative,
        LOCAL_LIGHT_SHADOW_MAX_NDC_BIAS);
}

// Compatibility adapter for call sites that do not yet provide a receiver
// normal term. A front-facing receiver preserves the previous behavior.
float ResolvePerspectiveShadowDepthBias(
    float viewDepth,
    float farPlane,
    float worldBias)
{
    return ResolvePerspectiveShadowDepthBias(
        viewDepth,
        farPlane,
        worldBias,
        1.0f);
}

#endif // SHADOW_DEPTH_BIAS_HLSLI
