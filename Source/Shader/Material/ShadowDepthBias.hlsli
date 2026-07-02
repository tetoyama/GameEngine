#ifndef SHADOW_DEPTH_BIAS_HLSLI
#define SHADOW_DEPTH_BIAS_HLSLI

#ifndef COMMON_DEFINE_H
#include "../commonDefine.h"
#endif

#ifndef COMMON_HLSL
#include "../common.hlsl"
#endif

#ifndef SHADOW_BIAS_MODE_LEGACY_NDC
#define SHADOW_BIAS_MODE_LEGACY_NDC (0)
#endif
#ifndef SHADOW_BIAS_MODE_WORLD_SPACE
#define SHADOW_BIAS_MODE_WORLD_SPACE (1)
#endif

bool UsesWorldSpaceShadowBias(LIGHT light)
{
    return (int) round(light.ShadowBias.z) == SHADOW_BIAS_MODE_WORLD_SPACE;
}

float3 ApplyShadowReceiverBias(
    float3 worldPos,
    float3 worldNormal,
    float3 directionToLight,
    float normalDotLight,
    LIGHT light)
{
    if (!UsesWorldSpaceShadowBias(light))
        return worldPos;

    const float3 safeNormal = normalize(worldNormal);
    const float3 safeDirectionToLight = normalize(directionToLight);
    const float depthBias = max(light.ShadowBias.x, 0.0f);
    const float normalBias = max(light.ShadowBias.y, 0.0f);
    const float grazingFactor = saturate(1.0f - normalDotLight);

    return worldPos +
        safeDirectionToLight * depthBias +
        safeNormal * (normalBias * grazingFactor);
}

float ResolveShadowCompareDepth(float projectedDepth, LIGHT light)
{
    if (UsesWorldSpaceShadowBias(light))
        return saturate(projectedDepth);

    return saturate(projectedDepth - max(light.ShadowBias.x, 0.0f));
}

float ResolveProjectedShadowDepthBias(float projectedDepthBias)
{
    if (!isfinite(projectedDepthBias))
        return 0.0f;

    return min(
        max(projectedDepthBias, 0.0f),
        LOCAL_LIGHT_SHADOW_MAX_NDC_BIAS);
}

float ResolveShadowReceiverAlignment(float receiverAlignment)
{
    return isfinite(receiverAlignment)
        ? saturate(receiverAlignment)
        : 1.0f;
}

float ResolveOrthographicShadowDepthBias(
    float legacyNdcBias,
    float receiverNdotL)
{
    const float baseNdcBias = ResolveProjectedShadowDepthBias(legacyNdcBias);

    const float safeNdotL = ResolveShadowReceiverAlignment(receiverNdotL);
    const float grazing = 1.0f - safeNdotL;
    const float slopeScale = min(
        1.0f + grazing * grazing * 8.0f,
        LOCAL_LIGHT_SHADOW_MAX_SLOPE_SCALE);

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
    const float baseNdcBias = ResolveProjectedShadowDepthBias(projectedDepthBias);
    if (baseNdcBias <= 0.0f)
        return 0.0f;

    const float nearPlane = LOCAL_LIGHT_SHADOW_NEAR_PLANE;
    const float safeFarInput = isfinite(farPlane)
        ? farPlane
        : nearPlane + LOCAL_LIGHT_SHADOW_MIN_DEPTH_SPAN;
    const float safeFar = max(
        safeFarInput,
        nearPlane + LOCAL_LIGHT_SHADOW_MIN_DEPTH_SPAN);
    const float safeViewDepth = isfinite(viewDepth)
        ? abs(viewDepth)
        : nearPlane;
    const float safeDepth = clamp(safeViewDepth, nearPlane, safeFar);
    const float referenceDepth = clamp(
        LOCAL_LIGHT_SHADOW_BIAS_REFERENCE_DEPTH,
        nearPlane,
        safeFar);

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

    const float safeNdotL = ResolveShadowReceiverAlignment(receiverNdotL);
    const float grazing = 1.0f - safeNdotL;
    const float slopeScale = min(
        1.0f + grazing * grazing * 8.0f,
        LOCAL_LIGHT_SHADOW_MAX_SLOPE_SCALE);

    const float distanceAdjustedBias =
        worldEquivalentBias * slopeScale * currentDerivative;

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
