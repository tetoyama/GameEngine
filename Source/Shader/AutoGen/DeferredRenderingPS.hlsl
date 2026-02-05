#include "../commonDefine.h"
#include "../common.hlsl"
#include "../Material/MaterialDefine.hlsli"
#include "../Material/DeferredFunc.hlsli"
#include "../Material/MaterialFunc.hlsli"

#include "../Material/UnlitShader.hlsli"


float4 main(PS_IN In) : SV_Target
{
    MaterialInput input = GetMaterialInput(In);
    float4 Result = float4(1, 0, 1, 1);

    switch (input.materialID)
    {
        case 0:
            Result = ShadeMaterial_Unlit(input);
            break;
        default:
            break;
    }

    return Result;
}
