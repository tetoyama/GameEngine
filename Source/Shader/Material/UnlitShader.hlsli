#ifndef SHADER_UNLIT_HLSLI
#define SHADER_UNLIT_HLSLI

#ifndef MATERIAL_DEFINE_HLSLI
#include "MaterialDefine.hlsli"
#include "MaterialFunc.hlsli"
#endif

float4 ShadeMaterial_Unlit(MaterialInput input)
{
    return input.baseColor;
}

#endif