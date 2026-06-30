#ifndef SHADOW_DEPTH_BIAS_HLSLI
#define SHADOW_DEPTH_BIAS_HLSLI

#ifndef COMMON_DEFINE_H
#include "../commonDefine.h"
#endif

float ResolveOrthographicShadowDepthBias(
    float projectedDepth,
    float legacyNdcBias)
{
    const float baseNdcBias = max(legacyNdcBias, 0.0f);

    // Directional / CSM use orthographic light projection, so Param.w is
    // already an NDC-depth bias. Add a receiver-plane slope term computed from
    // the actual projected depth gradient. This preserves the existing Param.w
    // contract and does not change LIGHT / constant-buffer layout.
    const float receiverSlope = max(
        abs(ddx(projectedDepth)),
        abs(ddy(projectedDepth)));

    const float slopeNdcBias = baseNdcBias > 0.0f
        ? min(receiverSlope * 2.0f, LOCAL_LIGHT_SHADOW_MAX_NDC_BIAS * 0.75f)
        : 0.0f;

    return min(
        baseNdcBias + slopeNdcBias,
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
    const float depthDerivative =
        (nearPlane * safeFar) /
        ((safeFar - nearPlane) * safeDepth * safeDepth);
    const float baseNdcBias = safeWorldBias * depthDerivative;

    const float projectedDepth =
        safeFar / (safeFar - nearPlane) -
        (nearPlane * safeFar) /
        ((safeFar - nearPlane) * safeDepth);
    const float receiverSlope = max(
        abs(ddx(projectedDepth)),
        abs(ddy(projectedDepth)));

    // Param.w == 0 must continue to disable receiver bias completely.
    const float slopeNdcBias = safeWorldBias > 0.0f
        ? min(receiverSlope * 2.0f, LOCAL_LIGHT_SHADOW_MAX_NDC_BIAS * 0.75f)
        : 0.0f;

    return min(
        baseNdcBias + slopeNdcBias,
        LOCAL_LIGHT_SHADOW_MAX_NDC_BIAS);
}

#endif // SHADOW_DEPTH_BIAS_HLSLI
