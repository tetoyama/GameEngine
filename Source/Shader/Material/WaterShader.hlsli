#ifndef MATERIAL_FUNC_HLSLI
#include "MaterialFunc.hlsli"
#endif

// 水面反射テクスチャ (TextureSlot_Reflection = 8)
// サンプラーは FowardFunc.hlsli / DeferredFunc.hlsli で宣言済みの LinearSampler (s0) を共用する
Texture2D WaterReflectionTex : register(t8);

//=============================================================================
// ShadeMaterial_Water
//  水面反射シェーダー
//  - Material.Roughness  : 反射強度 (1.0 = 最大反射, 0.0 = 反射なし)
//  - Material.AO         : 波による UV 歪み強度
//  - Material.Metallic   : 鏡面ハイライト強度 (0 = なし, 1 = 最大)
//  - Material.Pad0       : 鏡面ハイライトの鋭さ (Blinn-Phong shininess, 0 = デフォルト 64)
//  - MATERIAL_FLAG_USE_WATER_REFLECTION フラグが立っているときに反射サンプリング
//=============================================================================
float4 ShadeMaterial_Water(MaterialInput input)
{
    // ---- 水面固有パラメータを事前に取り出す ----
    // Material.Metallic は鏡面ハイライト強度として使用（水は非金属なので Metallic=0 で PBR 計算）
    float specStrength  = Material.Metallic;
    float specShininess = (Material.Pad0 < 1.0) ? 64.0 : max(Material.Pad0, 4.0);

    // PBR 計算では水を非金属（誘電体）として扱う
    input.Metallic = 0.0;

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
        // 反射テクスチャのサイズからスクリーン UV を正確に再計算する。
        // BaseColorTex がバインドされていない場合でも正しい UV が得られる。
        float2 reflTexSize;
        WaterReflectionTex.GetDimensions(reflTexSize.x, reflTexSize.y);
        float2 screenUV = (reflTexSize.x > 0.5)
                        ? (input.rawScreenPos / reflTexSize)
                        : input.screenUV;

        // 法線の XZ 成分で UV を歪ませる（波の揺れによる歪み）
        float  distortStrength = Material.AO;
        float2 distort         = input.normal.xz * distortStrength;

        // 画面空間 UV を Y 反転して反射テクスチャをサンプル
        float2 reflUV = float2(
            screenUV.x + distort.x,
            1.0 - screenUV.y + distort.y
        );
        reflUV = saturate(reflUV);

        float4 reflection = WaterReflectionTex.Sample(LinearSampler, reflUV);

        // フレネル項（水面に対して視線が浅い角度ほど反射が強い）
        float NdotV  = saturate(dot(normalize(input.normal), normalize(input.viewDir)));
        // 0.02 : 水の基本反射率（法線入射時の最小フレネル反射率）
        // pow(..., 4.0) : フレネル近似のべき乗（4.0 は水面に適した実験値）
        float fresnel = lerp(0.02, 1.0, pow(1.0 - NdotV, 4.0));

        // 反射強度 = フレネル × Roughness（Roughness を反射強度として使用）
        float reflStrength = fresnel * Material.Roughness;

        finalColor = lerp(litColor, reflection.rgb, reflStrength);
    }

    // ---- 水面鏡面ハイライト (Blinn-Phong) ----
    // Material.Metallic に格納された SpecularStrength が 0 より大きい場合のみ計算する。
    if (specStrength > 0.001)
    {
        float3 N = normalize(input.normal);
        float3 V = normalize(input.viewDir);

        float3 waterSpec = float3(0.0, 0.0, 0.0);

        // フレネル係数（鏡面ハイライトも視線角度で変化させる）
        float NdotV = saturate(dot(N, V));
        float fresnel = lerp(0.02, 1.0, pow(1.0 - NdotV, 4.0));

        [loop]
        for (int i = 0; i < ActiveLightCount; i++)
        {
            LIGHT light = Lights[i];
            if (light.Enable == 0)
                continue;

            float3 L;
            if (light.LightType == LIGHT_TYPE_DIRECTIONAL ||
                light.LightType == LIGHT_TYPE_DIRECTIONAL_CSM)
            {
                L = normalize(-light.Direction.xyz);
            }
            else
            {
                float3 toL = light.Position.xyz - input.worldPos;
                L = normalize(toL);
            }

            float NdotL = saturate(dot(N, L));
            float3 H    = normalize(V + L);
            float NdotH = saturate(dot(N, H));

            // Blinn-Phong 鏡面項
            float spec = pow(NdotH, specShininess) * NdotL;
            waterSpec += light.Diffuse.rgb * spec * specStrength * fresnel;
        }

        finalColor += waterSpec;
    }

    return float4(finalColor, input.baseColor.a);
}
