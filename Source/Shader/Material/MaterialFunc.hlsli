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
    float k = (a * a + 1.0) / 8.0; // 安定化
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

LightingResult ComputeLightingFromMaterialInput(MaterialInput input)
{
    LightingResult result;
    result.diffuse = 0.0;
    result.specular = 0.0;
    result.ambient = 0.0;

    float3 N = normalize(input.normal);
    float3 V = normalize(CameraPosition.xyz - input.worldPos);

    float roughness = saturate(input.Roughness);
    float metallic = saturate(input.Metallic);

    // 鏡面F0（baseColorは使わない）
    float3 F0 = lerp(float3(0.04, 0.04, 0.04), float3(1, 1, 1), metallic);

    int shadowMapNum = 0;

    [loop]
    for (int i = 0; i < ActiveLightCount; i++)
    {
        if (Lights[i].Enable == 0)
            continue;

        LIGHT light = Lights[i];

        float3 L;
        float attenuation = 1.0;

        if (light.LightType == LIGHT_TYPE_DIRECTIONAL)
        {
            L = normalize(-light.Direction.xyz);
        }
        else
        {
            float3 toL = light.Position.xyz - input.worldPos;
            float dist = length(toL);
            L = toL / max(dist, 0.001);
            attenuation = saturate(1.0 - dist / max(light.Param.x, 0.001));
        }

        float3 H = normalize(V + L);

        float NdotL = saturate(dot(N, L));
        float NdotV = saturate(dot(N, V));
        float NdotH = saturate(dot(N, H));

        if (NdotL <= 0.0 || NdotV <= 0.0)
            continue;

        float3 F = FresnelSchlick(saturate(dot(V, H)), F0);
        float G = G_Smith(N, V, L, roughness);
        float D = D_GTR2(NdotH, roughness);

        float3 specular =
            (D * G * F) / max(4.0 * NdotL * NdotV, 0.001);

        float3 kS = F;
        float3 kD = (1.0 - kS) * (1.0 - metallic);

        float shadow = 1.0;
        if (light.CastShadow)
        {
            shadow = ShadowFactor(input.worldPos, light, shadowMapNum);
            shadowMapNum++;
        }

        float3 radiance = light.Diffuse.rgb * attenuation * shadow;

        if (light.LightType == LIGHT_TYPE_DIRECTIONAL)
        {
            radiance *= 4.0;
        }

        result.diffuse += (NdotL) * radiance;
        result.specular += specular * radiance;
        result.ambient += light.Ambient.rgb;
    }

    // AO は光量に掛ける
    result.ambient *= input.AO;

    return result;
}

#endif
