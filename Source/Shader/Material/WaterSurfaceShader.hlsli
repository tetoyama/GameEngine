#ifndef MATERIAL_FUNC_HLSLI
#include "MaterialFunc.hlsli"
#endif

// Equirectangular マッピング: 反射方向 → UV
float2 EquirectangularUV(float3 R)
{
    float2 uv;
    uv.x = atan2(R.x, R.z) / (2.0 * PI) + 0.5;
    uv.y = acos(clamp(R.y, -1.0, 1.0)) / PI;
    return uv;
}

float4 ShadeMaterial_WaterSurface(MaterialInput materialInput)
{
    ShadowPCFParams pcf;
    pcf.KernelRadius = 2;
    pcf.StepTexel = 1;

    LightingResult lighting = ComputeLightingFromMaterialInput(materialInput, pcf);

    float3 V = normalize(materialInput.viewDir);
    float3 N = normalize(materialInput.normal);
    float NdotV = saturate(dot(N, V));

    // ---------- SkyBox 環境マップ反射 ----------
    float3 R = reflect(-V, N);
    float2 envUV = EquirectangularUV(R);

#ifdef FORWARD_FUNC_HLSLI
    // Forward: BaseColorTex にバインドされた SkyBox テクスチャから直接サンプリング
    float3 envColor = BaseColorTex.Sample(LinearSampler, envUV).rgb;
#else
    // Deferred: GBuffer の Albedo に格納済みの環境マップ色を使用
    float3 envColor = materialInput.baseColor.rgb;
#endif

    // 水の基本色（屈折光 = 水中から見える深い海の色）
    float3 shallowColor = float3(0.05, 0.15, 0.25);
    float3 deepColor    = float3(0.02, 0.05, 0.10);

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

    return float4(saturate(finalColor), 1.0);
}
