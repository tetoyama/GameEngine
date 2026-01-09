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
    float k = (a * a) * 0.5;
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

LightingResult ComputeLightingFromMaterialInput(
    MaterialInput input)
{
    LightingResult result;
    result.diffuse = 0.0;
    result.specular = 0.0;

    float3 worldPos = input.worldPos;
    float3 N = normalize(input.normal);
    float3 V = normalize(input.viewDir);
    float shininess = input.shininess;
    float3 baseColor = input.baseColor.rgb;

    [loop]
    for (int i = 0; i < LIGHT_MAX_COUNT; i++)
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
            float3 toL = light.Position.xyz - worldPos;
            float dist = length(toL);
            L = toL / max(dist, 0.001);
            attenuation =
                saturate(1.0 - dist / max(light.Param.x, 0.001));
        }

        float3 H = normalize(V + L);

        float NdotL = saturate(dot(N, L));
        float NdotV = saturate(dot(N, V));
        float NdotH = saturate(dot(N, H));
        float LdotH = saturate(dot(L, H));

        float roughness =
            max(1.0 - shininess / 128.0, 0.05);

        float3 F0 = float3(0.04, 0.04, 0.04);
        float3 F = FresnelSchlick(LdotH, F0);
        float G = G_Smith(N, V, L, roughness);
        float D = D_GTR2(NdotH, roughness);

        float shadow =
            ShadowFactor(worldPos, light, i);

        float3 spec =
            (D * F * G) /
            max(4.0 * NdotL * NdotV, 0.001);

        result.diffuse +=
            baseColor * light.Diffuse.rgb *
            NdotL * attenuation * shadow;

        result.specular +=
            spec * light.Diffuse.rgb *
            attenuation * shadow;
    }

    return result;
}
#endif
