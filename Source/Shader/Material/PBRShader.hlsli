#ifndef MATERIAL_FUNC_HLSLI
#include "MaterialFunc.hlsli"
#endif

float4 ShadeMaterial_PBR(MaterialInput materialInput)
{
    LightingResult lighting = ComputeLightingFromMaterialInput(materialInput);

    float3 baseColor = materialInput.baseColor.rgb;

    float3 finalColor =
          baseColor * lighting.diffuse
        + lighting.specular
        + baseColor * lighting.ambient
        + materialInput.Emissive.rgb;

    return float4(finalColor, materialInput.baseColor.a);
}

