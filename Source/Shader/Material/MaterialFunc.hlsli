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

        float3 L;
        float attenuation = 1.0;

        // --- ライト方向と減衰の計算 ---
        if (light.LightType == LIGHT_TYPE_DIRECTIONAL)
        {
            L = normalize(-light.Direction.xyz);
        }
        else if (light.LightType == LIGHT_TYPE_DIRECTIONAL_CSM)
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

        // --- シャドウマップ計算 ---
        float shadow = 1.0;
        if (light.CastShadow)
        {
            if (light.LightType == LIGHT_TYPE_DIRECTIONAL_CSM)
            {
                shadow = ShadowFactorCSM(input.worldPos, light, shadowParam);
                shadowMapNum += DIRECTIONAL_CSM_CASCADE_COUNT;
            }
            else
            {
                shadow = ShadowFactor(input.worldPos, light, shadowMapNum++, shadowParam);
            }
        }

        // ★【修正ポイント】影を真っ黒にしないための処理
        // shadowが0になっても、0.1の明るさを保証する。
        // これにより、影の中にも「弱い光」が残り、Toonシェーダー側で「影色」として描画されます。
        // (スペキュラ用には元のshadowを使うので、変数を分けます)
        float toonShadow = lerp(0.1, 1.0, shadow);

        // --- Diffuse計算 ---
        // 修正した toonShadow を使うことで、真っ黒回避
        float3 diffuse = light.Diffuse.rgb * attenuation * NdotL * toonShadow;

        // --- Specular計算 ---
        // スペキュラは影の中で光ってほしくないので、元の shadow (0になる) を使う
        float3 H = normalize(V + L);
        float NdotH = saturate(dot(N, H));
        float3 F = FresnelSchlick(saturate(dot(V, H)), F0);
        float G = G_Smith(N, V, L, roughness);
        float D = D_GTR2(NdotH, roughness);

        float3 specular = (D * G * F) / max(4.0 * NdotL * NdotV, 0.001)
                          * attenuation * shadow;

        // POINTライト等の補正
        if (light.LightType == LIGHT_TYPE_POINT && light.CastShadow)
        {
            diffuse /= 6.0f;
            specular /= 6.0f;
            light.Ambient.rgb /= 6.0f;
        }

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
