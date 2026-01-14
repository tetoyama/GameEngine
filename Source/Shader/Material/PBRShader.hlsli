#ifndef MATERIAL_FUNC_HLSLI
#include "MaterialFunc.hlsli"
#endif

float4 ShadeMaterial_PBR(MaterialInput materialInput)
{
    LightingResult lighting = ComputeLightingFromMaterialInput(materialInput);
    return float4((materialInput.baseColor.rgb + +lighting.specular) * lighting.diffuse + lighting.ambient, materialInput.baseColor.a);
}

