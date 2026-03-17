#ifndef MATERIAL_FUNC_HLSLI
#include "MaterialFunc.hlsli"
#endif

float4 ShadeMaterial_PBRToon(MaterialInput materialInput)
{
    ShadowPCFParams pcf;
    pcf.KernelRadius = 2;
    pcf.StepTexel = 1;
    
    LightingResult lighting = ComputeLightingFromMaterialInput(materialInput, pcf);

    // --- 1. Diffuseの多段階調 (境界にわずかな滑らかさ) ---
    float diffIntensity = length(lighting.diffuse);
    float toonDiff = 0.15; // 最低の影の濃さ
    toonDiff += smoothstep(0.20, 0.22, diffIntensity) * 0.35; // 影 -> 中間
    toonDiff += smoothstep(0.55, 0.57, diffIntensity) * 0.50; // 中間 -> 日向

    // --- 2. スペキュラ ---
    float specIntensity = length(lighting.specular);
    float toonSpec = smoothstep(0.48, 0.52, specIntensity);

    // --- 3. カラー合成 ---
    float3 baseColor = materialInput.baseColor.rgb;
    float3 shadowColor = baseColor * float3(0.6, 0.6, 0.8);
    float3 surfaceColor = lerp(shadowColor, baseColor, toonDiff);
    
    // Emissive contribution (HDR: not clamped, drives bloom)
    float3 emissive = materialInput.Emissive.rgb * materialInput.Emissive.a;
    
    // 最終カラー計算
    float3 finalColor = (lighting.diffuse * toonDiff + lighting.ambient) * surfaceColor
                        + (lighting.specular * toonSpec) + emissive;

    return float4(saturate(finalColor) + emissive, materialInput.baseColor.a);
}