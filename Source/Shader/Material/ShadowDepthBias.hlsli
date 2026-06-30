#ifndef SHADOW_DEPTH_BIAS_HLSLI
#define SHADOW_DEPTH_BIAS_HLSLI

#ifndef COMMON_DEFINE_H
#include "../commonDefine.h"
#endif

#ifndef COMMON_HLSL
#include "../common.hlsl"
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

    // Depth Bias moves the receiver toward the light in world space.
    // Normal Bias is strongest at grazing angles and zero when N faces L.
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

// Compatibility adapter for existing Deferred / Forward shadow functions.
// Legacy mode keeps Param.w as the original NDC bias. World mode stores 0 in
// Param.w because its offset has already been applied to the receiver position.
float ResolvePerspectiveShadowDepthBias(
    float viewDepth,
    float farPlane,
    float legacyNdcBias)
{
    return max(legacyNdcBias, 0.0f);
}

#endif // SHADOW_DEPTH_BIAS_HLSLI
