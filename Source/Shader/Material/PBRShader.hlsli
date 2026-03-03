#ifndef MATERIAL_FUNC_HLSLI
#include "MaterialFunc.hlsli"
#endif

// スカイスフィアのequirectangular(正距円筒図法)テクスチャを方向ベクトルでサンプリングするUV変換
float2 DirToSkyUV(float3 dir)
{
    float phi   = atan2(dir.x, dir.z);                    // -PI to PI
    float theta = asin(clamp(dir.y, -1.0, 1.0));          // -PI/2 to PI/2
    float u = phi / (2.0 * PI) + 0.5;
    float v = 0.5 - theta / PI;                            // Y軸反転でスカイスフィアUVと一致
    return float2(u, v);
}

float4 ShadeMaterial_PBR(MaterialInput materialInput)
{
    ShadowPCFParams pcf;
    pcf.KernelRadius = 2;   // 0 = no PCF
    pcf.StepTexel = 1;      // 1ピクセル間隔
    
    LightingResult lighting = ComputeLightingFromMaterialInput(materialInput, pcf);

    float3 baseColor = materialInput.baseColor.rgb;

    float3 specularColor = lighting.specular;

    // For metallic surfaces, environment map (sky sphere) replaces diffuse (avoids double-counting).
    // For dielectric surfaces (metallic=0), diffuse is unaffected.
    float envMapFactor = 0.0;
    if (materialInput.materialFlags & MATERIAL_FLAG_USE_ENVIRONMENT_MAP)
    {
        float3 N = normalize(materialInput.normal);
        float3 V = normalize(CameraPosition.xyz - materialInput.worldPos);
        float3 R = reflect(-V, N);
        // スカイスフィアのequirectangular UV でサンプリング (Roughnessによるミップ選択)
        float2 envUV = DirToSkyUV(R);
        float mipLevel = materialInput.Roughness * 8.0;
        float3 envColor = EnvironmentMap.SampleLevel(EnvSampler, envUV, mipLevel).rgb;
        envMapFactor = materialInput.Metallic;
        specularColor += envColor * envMapFactor;
    }

    float3 litColor = saturate(lighting.diffuse + lighting.ambient) * baseColor * (1.0 - envMapFactor) + specularColor;

    // Emissive contribution (HDR: not clamped, drives bloom)
    float3 emissive = materialInput.Emissive.rgb * materialInput.Emissive.a;

    return float4(saturate(litColor) + emissive, materialInput.baseColor.a);
}


