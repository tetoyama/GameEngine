#ifndef SHADER_UNLIT_HLSLI
#define SHADER_UNLIT_HLSLI

#ifndef MATERIAL_FUNC_HLSLI
#include "MaterialFunc.hlsli"
#endif

float4 ShadeMaterial_Unlit(MaterialInput input)
{
    return input.baseColor;
}

#endif