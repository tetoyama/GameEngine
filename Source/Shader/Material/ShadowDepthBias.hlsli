#ifndef SHADOW_DEPTH_BIAS_HLSLI
#define SHADOW_DEPTH_BIAS_HLSLI

#ifndef COMMON_DEFINE_H
#include "../commonDefine.h"
#endif

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

    // Receiver-plane slope bias.
    // A fixed bias is insufficient on surfaces nearly parallel to the light ray,
    // because projected depth changes more rapidly across adjacent pixels there.
    // Derivatives measure that projected-depth gradient directly, without adding
    // fields to LIGHT or changing the Param.w contract.
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
