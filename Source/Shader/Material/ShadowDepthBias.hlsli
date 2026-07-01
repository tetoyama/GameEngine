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
    float worldBias,
    float receiverNdotL)
{
    const float nearPlane = LOCAL_LIGHT_SHADOW_NEAR_PLANE;
    const float safeFar = max(
        farPlane,
        nearPlane + LOCAL_LIGHT_SHADOW_MIN_DEPTH_SPAN);
    const float safeDepth = clamp(abs(viewDepth), nearPlane, safeFar);

    const float safeNdotL = saturate(receiverNdotL);
    const float grazing = 1.0f - safeNdotL;
    const float slopeScale = 1.0f + grazing * grazing * 8.0f;
    const float effectiveWorldBias = max(worldBias, 0.0f) * slopeScale;

    const float depthDerivative =
        (nearPlane * safeFar) /
        ((safeFar - nearPlane) * safeDepth * safeDepth);

    return min(
        effectiveWorldBias * depthDerivative,
        LOCAL_LIGHT_SHADOW_MAX_NDC_BIAS);
}

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

float ResolveRadialShadowDepthBias(
    float farPlane,
    float worldBias,
    float receiverNdotL)
{
    const float safeFar = max(
        farPlane,
        LOCAL_LIGHT_SHADOW_NEAR_PLANE +
            LOCAL_LIGHT_SHADOW_MIN_DEPTH_SPAN);

    const float safeNdotL = saturate(receiverNdotL);
    const float grazing = 1.0f - safeNdotL;
    const float slopeScale = 1.0f + grazing * grazing * 4.0f;
    const float effectiveWorldBias = max(worldBias, 0.0f) * slopeScale;

    // The perspective shadow map stores distance(light, fragment) / far.
    // Convert the world-unit bias to the same face-independent linear space.
    return min(
        effectiveWorldBias / safeFar,
        LOCAL_LIGHT_SHADOW_MAX_NDC_BIAS);
}

#endif // SHADOW_DEPTH_BIAS_HLSLI
