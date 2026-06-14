#include "../commonDefine.h"
#include "../common.hlsl"
#include "../Material/MaterialDefine.hlsli"
#include "../Material/DeferredFunc.hlsli"
#include "../Material/MaterialFunc.hlsli"
#include "../Material/RimToonShader.hlsli"

float4 main(PS_IN In) : SV_Target
{
    if (GetMaterialID(In) != 4u)
    {
        discard;
    }

    MaterialInput input = GetMaterialInput(In);
    return ShadeMaterial_RimToon(input);
}
