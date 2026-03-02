#ifndef MATERIAL_FUNC_HLSLI
#include "MaterialFunc.hlsli"
#endif

float4 ShadeMaterial_WaterSurface(MaterialInput materialInput)
{
    ShadowPCFParams pcf;
    pcf.KernelRadius = 2;
    pcf.StepTexel = 1;

    LightingResult lighting = ComputeLightingFromMaterialInput(materialInput, pcf);

    float3 baseColor = materialInput.baseColor.rgb;

    // Fresnel for water (F0 ~ 0.02)
    float3 V = normalize(materialInput.viewDir);
    float3 N = normalize(materialInput.normal);
    float NdotV = saturate(dot(N, V));
    float fresnel = 0.02 + 0.98 * pow(1.0 - NdotV, 5.0);

    // Diffuse contribution (reduced at grazing angles due to Fresnel)
    float3 diffuse = saturate(lighting.diffuse + lighting.ambient) * baseColor * (1.0 - fresnel);

    // Specular highlights
    float3 specular = lighting.specular;

    // Reflection contribution (environment mapped color stored in albedo by GBuffer)
    float3 reflection = baseColor * fresnel;

    float3 finalColor = diffuse + specular + reflection;

    return float4(saturate(finalColor), materialInput.baseColor.a);
}
