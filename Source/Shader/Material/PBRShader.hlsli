#ifndef MATERIAL_FUNC_HLSLI
#include "MaterialFunc.hlsli"
#endif

float4 ShadeMaterial_PBR(MaterialInput materialInput)
{
    LightingResult lighting = ComputeLightingFromMaterialInput(materialInput);

    float3 diffuseColor = materialInput.baseColor.rgb * lighting.diffuse;
    float3 specularColor = lighting.specular;

    float3 ambientColor = materialInput.baseColor.rgb * lighting.ambient;

    float3 finalColor = diffuseColor + specularColor + ambientColor;

    finalColor = saturate(finalColor);

    return float4(finalColor, materialInput.baseColor.a);
}
