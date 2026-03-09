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


LightingResult ComputeLightingFromMaterialInput(MaterialInput input, ShadowPCFParams shadowParam)
{
    LightingResult result = (LightingResult) 0;

    float3 N = normalize(input.normal);
    float3 V = normalize(CameraPosition.xyz - input.worldPos);

    float roughness = saturate(input.Roughness);
    float metallic = saturate(input.Metallic);

    float3 F0 = lerp(float3(0.04, 0.04, 0.04), float3(1.0, 1.0, 1.0), metallic);

    int shadowMapNum = 0;

    [loop]
    for (int i = 0; i < ActiveLightCount; i++)
    {
        LIGHT light = Lights[i];
        if (light.Enable == 0)
            continue;

        if (light.Dummy >= 2 || (light.LightType == LIGHT_TYPE_POINT && light.Dummy <= -2))
            continue;

        float3 L;
        float attenuation = 1.0;

        // -----------------------------
        // Light direction
        // -----------------------------

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

            if (light.LightType == LIGHT_TYPE_SPOT)
            {
                float3 spotDir = normalize(-light.Direction.xyz);

                float cosTheta = dot(L, spotDir);

                float innerCos = cos(radians(light.Param.y));
                float outerCos = cos(radians(light.Param.z));

                attenuation *= saturate((cosTheta - outerCos) / max(innerCos - outerCos, 0.001));
            }
        }

        float NdotL = saturate(dot(N, L));
        float NdotV = saturate(dot(N, V));

        if (NdotL <= 0)
            continue;

        // -----------------------------
        // Shadow
        // -----------------------------

        float shadow = 1.0;

        if (light.CastShadow)
        {
            if (light.Dummy == 1)
            {
                // 壊れたデータで 0 が入っても atlas 進行が崩れないよう最小値を 1 に保つ。
                int cascadeCount = max((int) round(light.Position.w), 1);

                shadow = ShadowFactorCascades(
                    input.worldPos,
                    i,
                    cascadeCount,
                    shadowMapNum,
                    shadowParam);

                shadowMapNum += cascadeCount;
            }
            else if (light.LightType == LIGHT_TYPE_POINT && light.Dummy == -1)
            {
                // 先頭 face の Position.w に実 face 数を格納。破損時でも最低 1 face として扱う。
                int pointFaceCount = max((int) round(light.Position.w), 1);

                shadow = ShadowFactorPoint(
                    input.worldPos,
                    i,
                    pointFaceCount,
                    shadowMapNum,
                    shadowParam);

                shadowMapNum += pointFaceCount;
            }
            else
            {
                shadow = ShadowFactor(
                    input.worldPos,
                    light,
                    shadowMapNum++,
                    shadowParam);
            }
        }

        // -----------------------------
        // Toon shadow
        // -----------------------------

        float toonShadow = lerp(0.1, 1.0, shadow);

        // -----------------------------
        // Diffuse
        // -----------------------------

        float3 diffuse =
            light.Diffuse.rgb *
            attenuation *
            NdotL *
            toonShadow;

        // -----------------------------
        // Specular
        // -----------------------------

        float3 H = normalize(V + L);

        float NdotH = saturate(dot(N, H));

        float3 F = FresnelSchlick(saturate(dot(V, H)), F0);

        float G = G_Smith(N, V, L, roughness);

        float D = D_GTR2(NdotH, roughness);

        float3 specBRDF = (D * G * F) / max(4.0 * NdotL * NdotV, 0.001);

        // roughnessが高いと影の影響を弱くする
        float specularShadow = lerp(1.0, shadow, saturate(1.0 - roughness));

        float3 specular =
            specBRDF *
            light.Diffuse.rgb *
            attenuation *
            NdotL *
            specularShadow;

        result.diffuse += diffuse;
        result.specular += specular;
        result.ambient += light.Ambient.rgb;
    }

    return result;
}
float Quantize4(float v)
{
    v = saturate(v);
    return floor(v * 4.0) / 3.0;
}

#endif
