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

    float3 litColor = saturate(lighting.diffuse + lighting.ambient) * baseColor + specularColor;

    // Emissive contribution (HDR: not clamped, drives bloom)
    float3 emissive = materialInput.Emissive.rgb * materialInput.Emissive.a;

    return float4(saturate(litColor) + emissive, materialInput.baseColor.a);
}


