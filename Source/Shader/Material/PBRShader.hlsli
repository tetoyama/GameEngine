#ifndef SHADER_PBR_HLSLI
#define SHADER_PBR_HLSLI

#ifndef MATERIAL_FUNC_HLSLI
#include "MaterialFunc.hlsli"
#endif

float4 ShadeMaterial_PBR(MaterialInput input)
{
    LightingResult lighting = ComputeLightingFromMaterialInput(input);
    return float4(input.baseColor.rgb * lighting.diffuse + lighting.specular, input.baseColor.a);
}
#endif
