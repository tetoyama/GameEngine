#include "../commonDefine.h"
#include "../common.hlsl"
#include "../Material/MaterialDefine.hlsli"
#include "../Material/DeferredFunc.hlsli"
#include "../Material/MaterialFunc.hlsli"

#include "../Material/UnlitShader.hlsli"
#include "../Material/PBRShader.hlsli"
#include "../Material/PBRToonShader.hlsli"

float4 main(PS_IN In) : SV_Target
{
    MaterialInput input = GetMaterialInput(In);
    float4 Result = float4(1, 0, 1, 1);

    [branch] if (input.materialID == 0)
    {
        Result = ShadeMaterial_Unlit(input);
    }
    else if (input.materialID == 1)
    {
        Result = ShadeMaterial_PBR(input);
    }
    else if (input.materialID == 2)
    {
        Result = ShadeMaterial_PBRToon(input);
    }
    else { /* default */ }

    return Result;
}
