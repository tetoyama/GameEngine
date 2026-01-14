#ifndef MATERIAL_FUNC_HLSLI
#include "MaterialFunc.hlsli"
#endif

float4 ShadeMaterial_Unlit(MaterialInput input)
{
    return input.baseColor;
}
