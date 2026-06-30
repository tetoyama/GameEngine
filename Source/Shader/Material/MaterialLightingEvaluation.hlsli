#ifndef MATERIAL_LIGHTING_EVALUATION_HLSLI
#define MATERIAL_LIGHTING_EVALUATION_HLSLI

#ifndef SHADOW_DEPTH_BIAS_HLSLI
#include "ShadowDepthBias.hlsli"
#endif

int ResolvePackedLightEntrySpan(
    LIGHT light,
    int firstEntryIndex,
    int activeEntryCount)
{
    int remainingEntries = max(activeEntryCount - firstEntryIndex, 1);
    int span = 1;

    if (light.LightType == LIGHT_TYPE_DIRECTIONAL_CSM && light.Dummy == 1)
    {
        span = max((int) round(light.Position.w), 1);
    }
    else if (light.LightType == LIGHT_TYPE_POINT && light.Dummy == -1)
    {
        span = max((int) round(light.Position.w), 1);
    }

    return min(span, remainingEntries);
}

bool ShouldEvaluateShadow(LIGHT light)
{
    if (light.CastShadow == 0)
        return false;

    if ((LightingDebugFlags & LIGHTING_DEBUG_FLAG_DISABLE_SHADOWS) != 0u)
        return false;

    if (light.LightType == LIGHT_TYPE_DIRECTIONAL_CSM &&
        (LightingDebugFlags & LIGHTING_DEBUG_FLAG_DISABLE_CSM_SHADOWS) != 0u)
        return false;

    if (light.LightType == LIGHT_TYPE_POINT &&
        (LightingDebugFlags & LIGHTING_DEBUG_FLAG_DISABLE_POINT_SHADOWS) != 0u)
        return false;

    return true;
}

LightingResult ComputeLightingFromMaterialInput(
    MaterialInput input,
    ShadowPCFParams shadowParam)
{
    LightingResult result = (LightingResult) 0;

    float3 N = normalize(input.normal);
    float3 V = normalize(CameraPosition.xyz - input.worldPos);

    float roughness = saturate(input.Roughness);
    float metallic = saturate(input.Metallic);
    float3 F0 = lerp(
        float3(0.04, 0.04, 0.04),
        float3(1.0, 1.0, 1.0),
        metallic);

    int activeEntryCount = clamp(ActiveLightCount, 0, LIGHT_MAX_COUNT);
    int entryIndex = 0;
    int shadowAtlasOffset = 0;

    [loop]
    while (entryIndex < activeEntryCount)
    {
        int currentEntryIndex = entryIndex;
        LIGHT light = Lights[currentEntryIndex];
        int entrySpan = ResolvePackedLightEntrySpan(
            light,
            currentEntryIndex,
            activeEntryCount);
        int currentShadowAtlasOffset = shadowAtlasOffset;

        entryIndex += entrySpan;
        if (light.CastShadow)
            shadowAtlasOffset += entrySpan;

        if (light.Enable == 0)
            continue;
        if (light.LightType == LIGHT_TYPE_POINT && light.Dummy < -1)
            continue;
        if (light.LightType == LIGHT_TYPE_DIRECTIONAL_CSM && light.Dummy > 1)
            continue;

        float3 L;
        float attenuation = 1.0;

        if (light.LightType == LIGHT_TYPE_DIRECTIONAL ||
            light.LightType == LIGHT_TYPE_DIRECTIONAL_CSM)
        {
            L = normalize(-light.Direction.xyz);
        }
        else
        {
            float3 toL = light.Position.xyz - input.worldPos;
            float dist = length(toL);
            L = toL / max(dist, 0.001);
            attenuation = saturate(
                1.0 - dist / max(light.Param.x, 0.001));

            if (light.LightType == LIGHT_TYPE_SPOT)
            {
                float3 spotDir = normalize(-light.Direction.xyz);
                float cosTheta = dot(L, spotDir);
                float innerCos = cos(radians(light.Param.y));
                float outerCos = cos(radians(light.Param.z));
                attenuation *= saturate(
                    (cosTheta - outerCos) /
                    max(innerCos - outerCos, 0.001));
            }
        }

        float NdotL = saturate(dot(N, L));
        float NdotV = saturate(dot(N, V));
        if (NdotL <= 0)
            continue;

        if (attenuation <= 0.0f)
        {
            result.ambient += light.Ambient.rgb;
            continue;
        }

        float shadow = 1.0;
        if (ShouldEvaluateShadow(light))
        {
            const float3 shadowWorldPos = ApplyShadowReceiverBias(
                input.worldPos,
                N,
                L,
                NdotL,
                light);

            if (light.LightType == LIGHT_TYPE_DIRECTIONAL_CSM &&
                light.Dummy == 1)
            {
                shadow = ShadowFactorCascades(
                    shadowWorldPos,
                    currentEntryIndex,
                    entrySpan,
                    currentShadowAtlasOffset,
                    shadowParam);
            }
            else if (light.LightType == LIGHT_TYPE_POINT &&
                     light.Dummy == -1)
            {
                shadow = ShadowFactorPoint(
                    shadowWorldPos,
                    currentEntryIndex,
                    entrySpan,
                    currentShadowAtlasOffset,
                    shadowParam);
            }
            else
            {
                shadow = ShadowFactor(
                    shadowWorldPos,
                    light,
                    currentShadowAtlasOffset,
                    shadowParam);
            }
        }

        float toonShadow = lerp(0.1, 1.0, shadow);
        float3 diffuse =
            light.Diffuse.rgb *
            attenuation *
            NdotL *
            toonShadow;

        float3 H = normalize(V + L);
        float NdotH = saturate(dot(N, H));
        float3 F = FresnelSchlick(saturate(dot(V, H)), F0);
        float G = G_Smith(N, V, L, roughness);
        float D = D_GTR2(NdotH, roughness);
        float3 specBRDF =
            (D * G * F) /
            max(4.0 * NdotL * NdotV, 0.001);

        float specularShadow =
            lerp(1.0, shadow, saturate(1.0 - roughness));
        float3 specular =
            specBRDF *
            light.Diffuse.rgb *
            attenuation *
            NdotL *
            specularShadow;

        result.diffuse += saturate(diffuse);
        result.specular += specular;
        result.ambient += light.Ambient.rgb;
    }

    return result;
}

#endif // MATERIAL_LIGHTING_EVALUATION_HLSLI
