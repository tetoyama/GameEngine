#ifndef MATERIAL_FUNC_HLSLI
#include "MaterialFunc.hlsli"
#endif

// 水面反射テクスチャ (TextureSlot_Reflection = 8)
Texture2D WaterReflectionTex : register(t8);
SamplerState WaterSampler    : register(s0);

//=============================================================================
// ShadeMaterial_Water
//  水面反射シェーダー
//  - Material.Roughness : 反射強度 (1.0 = 最大反射, 0.0 = 反射なし)
//  - Material.AO        : 波による UV 歪み強度
//  - MATERIAL_FLAG_USE_WATER_REFLECTION フラグが立っているときに反射サンプリング
//=============================================================================
float4 ShadeMaterial_Water(MaterialInput input)
{
    // ---- ライティング ----
    ShadowPCFParams pcf;
    pcf.KernelRadius = 1;
    pcf.StepTexel    = 1.0;

    LightingResult lighting = ComputeLightingFromMaterialInput(input, pcf);

    float3 baseColor = input.baseColor.rgb;
    float3 litColor  = saturate(lighting.diffuse + lighting.ambient) * baseColor
                     + lighting.specular;

    // ---- 反射サンプリング ----
    float3 finalColor = litColor;

    if (Material.MaterialFlags & MATERIAL_FLAG_USE_WATER_REFLECTION)
    {
        // 法線の XZ 成分で UV を歪ませる（波の揺れによる歪み）
        float  distortStrength = Material.AO;
        float2 distort         = input.normal.xz * distortStrength;

        // 画面空間 UV を Y 反転して反射テクスチャをサンプル
        float2 reflUV = float2(
            input.screenUV.x + distort.x,
            1.0 - input.screenUV.y + distort.y
        );
        reflUV = saturate(reflUV);

        float4 reflection = WaterReflectionTex.Sample(WaterSampler, reflUV);

        // フレネル項（水面に対して視線が浅い角度ほど反射が強い）
        float NdotV  = saturate(dot(normalize(input.normal), normalize(input.viewDir)));
        // 0.02 : 水の基本反射率（法線入射時の最小フレネル反射率）
        // pow(..., 4.0) : フレネル近似のべき乗（4.0 は水面に適した実験値）
        float fresnel = lerp(0.02, 1.0, pow(1.0 - NdotV, 4.0));

        // 反射強度 = フレネル × Roughness（Roughness を反射強度として使用）
        float reflStrength = fresnel * Material.Roughness;

        finalColor = lerp(litColor, reflection.rgb, reflStrength);
    }

    return float4(finalColor, input.baseColor.a);
}
