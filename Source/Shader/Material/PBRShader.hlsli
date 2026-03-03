#ifndef MATERIAL_FUNC_HLSLI
#include "MaterialFunc.hlsli"
#endif

float4 ShadeMaterial_PBR(MaterialInput materialInput)
{
    ShadowPCFParams pcf;
    pcf.KernelRadius = 2;   // 0 = no PCF
    pcf.StepTexel = 1;      // 1ピクセル間隔
    
    LightingResult lighting = ComputeLightingFromMaterialInput(materialInput, pcf);

    float3 baseColor = materialInput.baseColor.rgb;

    float3 specularColor = lighting.specular;

    // For metallic surfaces, environment map replaces diffuse (avoids double-counting).
    // For dielectric surfaces (metallic=0), diffuse is unaffected.
    float envMapFactor = 0.0;
    if (materialInput.materialFlags & MATERIAL_FLAG_USE_ENVIRONMENT_MAP)
    {
        float3 N = normalize(materialInput.normal);
        float3 V = normalize(CameraPosition.xyz - materialInput.worldPos);
        float3 R = reflect(-V, N);
        float mipLevel = materialInput.Roughness * 8.0;
        envMapFactor = materialInput.Metallic;
        float3 envColor = EnvironmentMap.SampleLevel(EnvSampler, R, mipLevel).rgb;
        specularColor += envColor * envMapFactor;
    }

    float3 litColor = saturate(lighting.diffuse + lighting.ambient) * baseColor * (1.0 - envMapFactor) + specularColor;

    // Emissive contribution (HDR: not clamped, drives bloom)
    float3 emissive = materialInput.Emissive.rgb * materialInput.Emissive.a;

    return float4(saturate(litColor) + emissive, materialInput.baseColor.a);
}


