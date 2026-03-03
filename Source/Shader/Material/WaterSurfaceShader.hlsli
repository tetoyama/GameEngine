#ifndef MATERIAL_FUNC_HLSLI
#include "MaterialFunc.hlsli"
#endif

float4 ShadeMaterial_WaterSurface(MaterialInput materialInput)
{
    ShadowPCFParams pcf;
    pcf.KernelRadius = 2;
    pcf.StepTexel = 1;

    LightingResult lighting = ComputeLightingFromMaterialInput(materialInput, pcf);

    // GBuffer の albedo には環境マップ（空テクスチャ）の反射色が格納されている
    float3 envColor = materialInput.baseColor.rgb;

    // 水の基本色（屈折光 = 水中から見える深い海の色）
    float3 shallowColor = float3(0.05, 0.15, 0.25);
    float3 deepColor    = float3(0.02, 0.05, 0.10);

    // 視線角度による水深感の変化
    float3 V = normalize(materialInput.viewDir);
    float3 N = normalize(materialInput.normal);
    float NdotV = saturate(dot(N, V));

    // 急角度ほど深い色、浅い角度ほど明るい色
    float3 waterColor = lerp(deepColor, shallowColor, NdotV);

    // Fresnel (水面: F0 ≈ 0.02)
    float fresnel = 0.02 + 0.98 * pow(1.0 - NdotV, 5.0);

    // 屈折成分: ライティングされた水の色
    float3 refraction = saturate(lighting.diffuse + lighting.ambient) * waterColor;

    // 反射成分: 環境マップの色（Roughness に応じて減衰）
    static const float ENV_ROUGHNESS_ATTEN = 0.6;
    float roughness = saturate(materialInput.Roughness);
    float envStrength = 1.0 - roughness * ENV_ROUGHNESS_ATTEN;
    float3 reflection = envColor * envStrength;

    // スペキュラハイライト（太陽光のきらめき）
    float3 specular = lighting.specular;

    // Fresnel で屈折と反射をブレンド
    float3 finalColor = lerp(refraction, reflection, fresnel) + specular;

    return float4(saturate(finalColor), 0.95);
}
