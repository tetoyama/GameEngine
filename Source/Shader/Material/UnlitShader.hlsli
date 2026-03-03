#ifndef MATERIAL_FUNC_HLSLI
#include "MaterialFunc.hlsli"
#endif

float4 ShadeMaterial_Unlit(MaterialInput input)
{
    // Emissive contribution (HDR: not clamped, drives bloom)
    float3 emissive = input.Emissive.rgb * input.Emissive.a;
    return float4(input.baseColor.rgb + emissive, input.baseColor.a);
}
