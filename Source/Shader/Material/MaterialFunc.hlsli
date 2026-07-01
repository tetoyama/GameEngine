#ifndef MATERIAL_FUNC_HLSLI
#define MATERIAL_FUNC_HLSLI

#ifndef MATERIAL_DEFINE_HLSLI

#include "MaterialDefine.hlsli"

#ifdef FORWARD_FUNC_HLSLI
#include "FowardFunc.hlsli"
#else
#include "DeferredFunc.hlsli"
#endif//FORWARD_FUNC_HLSLI

#endif //MATERIAL_DEFINE_HLSLI

#ifndef COMMON_DEFINE_HLSLI
#include "../commonDefine.h"
#endif

#ifndef COMMON_HLSL
#include "../common.hlsl"
#endif

// ===== BRDF =====
float3 FresnelSchlick(float cosTheta, float3 F0)
{
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

float SchlickGGX(float NdotV, float roughness)
{
    float a = roughness;
    float k = (a * a + 1.0) / 8.0;
    return NdotV / (NdotV * (1.0 - k) + k);
}

float G_Smith(float3 N, float3 V, float3 L, float roughness)
{
    float NdotV = saturate(dot(N, V));
    float NdotL = saturate(dot(N, L));
    return SchlickGGX(NdotV, roughness) * SchlickGGX(NdotL, roughness);
}

float D_GTR2(float NdotH, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float d = (NdotH * NdotH) * (a2 - 1.0) + 1.0;
    return a2 / (PI * d * d);
}

int ResolvePackedLightEntrySpan(LIGHT light, int firstEntryIndex, int activeEntryCount)
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

LightingResult ComputeLightingFromMaterialInput(MaterialInput input, ShadowPCFParams shadowParam)
{
    LightingResult result = (LightingResult) 0;

    float3 N = normalize(input.normal);
    float3 V = normalize(CameraPosition.xyz - input.worldPos);

    float roughness = saturate(input.Roughness);
    float metallic = saturate(input.Metallic);

    float3 F0 = lerp(float3(0.04, 0.04, 0.04), float3(1.0, 1.0, 1.0), metallic);

    int activeEntryCount = clamp(ActiveLightCount, 0, LIGHT_MAX_COUNT);
    int entryIndex = 0;

    [loop]
    while (entryIndex < activeEntryCount)
    {
        int currentEntryIndex = entryIndex;
        LIGHT light = Lights[currentEntryIndex];
        int entrySpan = ResolvePackedLightEntrySpan(
            light,
            currentEntryIndex,
            activeEntryCount);

        entryIndex += entrySpan;

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
            attenuation = saturate(1.0 - dist / max(light.Param.x, 0.001));

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

        // Point / Spotの影響範囲外ではDiffuseとSpecularが必ず0になる。
        // 既存のAmbient加算だけを保持し、Shadow評価とBRDF計算を省略する。
        if (attenuation <= 0.0f)
        {
            result.ambient += light.Ambient.rgb;
            continue;
        }

        float shadow = 1.0;

        if (ShouldEvaluateShadow(light))
        {
            if (light.LightType == LIGHT_TYPE_DIRECTIONAL_CSM && light.Dummy == 1)
            {
                shadow = ShadowFactorCascades(
                    input.worldPos,
                    currentEntryIndex,
                    entrySpan,
                    NdotL,
                    shadowParam);
            }
            else if (light.LightType == LIGHT_TYPE_POINT && light.Dummy == -1)
            {
                shadow = ShadowFactorPoint(
                    input.worldPos,
                    currentEntryIndex,
                    entrySpan,
                    shadowParam);
            }
            else
            {
                shadow = ShadowFactor(
                    input.worldPos,
                    light,
                    currentEntryIndex,
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

float Quantize4(float v)
{
    v = saturate(v);
    v = floor(v * 4.0) / 3.0;
    v = saturate(saturate(saturate(v - 0.1) + 0.2) - 0.1);
    return v;
}

#endif