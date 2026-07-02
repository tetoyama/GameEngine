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
    const float baseNdcBias = min(
        max(projectedDepthBias, 0.0f),
        LOCAL_LIGHT_SHADOW_MAX_NDC_BIAS);

    const float nearPlane = LOCAL_LIGHT_SHADOW_NEAR_PLANE;
    const float safeFar = max(
        farPlane,
        nearPlane + LOCAL_LIGHT_SHADOW_MIN_DEPTH_SPAN);
    const float safeDepth = clamp(abs(viewDepth), nearPlane, safeFar);
    const float referenceDepth = clamp(
        LOCAL_LIGHT_SHADOW_BIAS_REFERENCE_DEPTH,
        nearPlane,
        safeFar);

    // DirectX LH perspective depth:
    // z_ndc = far/(far-near) - near*far/((far-near)*viewZ)
    // Its derivative is proportional to 1/viewZ^2. A fixed NDC bias therefore
    // becomes an increasingly large world-space offset at long distance and
    // eventually skips over the caster. Convert the persisted bias at the
    // reference depth into a world-space-equivalent bias, then project it at
    // the current receiver depth.
    const float projectionScale =
        (nearPlane * safeFar) /
        max(safeFar - nearPlane, 0.000001f);
    const float referenceDerivative =
        projectionScale /
        max(referenceDepth * referenceDepth, 0.000001f);
    const float currentDerivative =
        projectionScale /
        max(safeDepth * safeDepth, 0.000001f);

    const float worldEquivalentBias =
        baseNdcBias /
        max(referenceDerivative, 0.000001f);

    const float safeNdotL = saturate(receiverNdotL);
    const float grazing = 1.0f - safeNdotL;
    const float slopeScale = min(
        1.0f + grazing * grazing * 8.0f,
        LOCAL_LIGHT_SHADOW_MAX_SLOPE_SCALE);

    const float distanceAdjustedBias =
        worldEquivalentBias * slopeScale * currentDerivative;

    // Never exceed the legacy fixed-NDC value at close range. This preserves
    // the self-shadow fix while preventing distant shadows from disappearing.
    return min(baseNdcBias, distanceAdjustedBias);
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
