#ifndef SHADER_PBR_HLSLI
#define SHADER_PBR_HLSLI

#ifndef MATERIAL_FUNC_HLSLI
#include "MaterialFunc.hlsli"
#endif

float4 ShadeMaterial_PBR(MaterialInput materialInput)
{
    //return float4(materialInput.worldPos, 1);
    //return float4(materialInput.viewDir, 1);
    
    LightingResult lighting = ComputeLightingFromMaterialInput(materialInput);
    return float4((materialInput.baseColor.rgb + +lighting.specular) * lighting.diffuse, materialInput.baseColor.a);
}
#endif
