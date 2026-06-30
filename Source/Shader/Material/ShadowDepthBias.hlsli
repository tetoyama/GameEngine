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
    // receiver is nearly parallel to the light direction. This keeps LIGHT and
    // constant-buffer layout unchanged and avoids derivative-based side effects.
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
    float worldBias)
{
    const float nearPlane = LOCAL_LIGHT_SHADOW_NEAR_PLANE;
    const float safeFar = max(
        farPlane,
        nearPlane + LOCAL_LIGHT_SHADOW_MIN_DEPTH_SPAN);
    const float safeDepth = clamp(abs(viewDepth), nearPlane, safeFar);
    const float safeWorldBias = max(worldBias, 0.0f);

    // DirectX LH perspective depth:
    // z_ndc = far/(far-near) - near*far/((far-near)*viewZ)
    // Convert the configured world-distance bias to NDC at this receiver depth.
    // Keep local light bias distance-based only; derivative bias can vary across
    // cubemap faces and was causing Point Light regressions.
    const float depthDerivative =
        (nearPlane * safeFar) /
        ((safeFar - nearPlane) * safeDepth * safeDepth);

    return min(
        safeWorldBias * depthDerivative,
        LOCAL_LIGHT_SHADOW_MAX_NDC_BIAS);
}

#endif // SHADOW_DEPTH_BIAS_HLSLI
