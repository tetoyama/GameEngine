#ifndef MATERIAL_FUNC_HLSLI
#include "MaterialFunc.hlsli"
#endif

float4 ShadeMaterial_WaterSurface(MaterialInput materialInput)
{
    ShadowPCFParams pcf;
    pcf.KernelRadius = 2;
    pcf.StepTexel = 1;

    LightingResult lighting = ComputeLightingFromMaterialInput(materialInput, pcf);

    // GBuffer の albedo には環境マップの反射色が格納されている
    float3 envColor = materialInput.baseColor.rgb;

    // 水の基本色（屈折光 = 水中から見える色）
    float3 waterColor = float3(0.02, 0.08, 0.15);

    // Fresnel (水面: F0 ≈ 0.02)
    float3 V = normalize(materialInput.viewDir);
    float3 N = normalize(materialInput.normal);
    float NdotV = saturate(dot(N, V));
    float fresnel = 0.02 + 0.98 * pow(1.0 - NdotV, 5.0);

    // 屈折成分: ライティングされた水の色
    float3 refraction = saturate(lighting.diffuse + lighting.ambient) * waterColor;

    // 反射成分: 環境マップの色
    float3 reflection = envColor;

    // スペキュラハイライト
    float3 specular = lighting.specular;

    // Fresnel で屈折と反射をブレンド
    float3 finalColor = lerp(refraction, reflection, fresnel) + specular;

    return float4(saturate(finalColor), 0.95);
}
