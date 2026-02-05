#include "../commonDefine.h"
#include "../common.hlsl"
#include "../Material/MaterialDefine.hlsli"
#include "../Material/DeferredFunc.hlsli"
#include "../Material/MaterialFunc.hlsli"

#include "../Material/UnlitShader.hlsli"
#include "../Material/PBRShader.hlsli"
#include "../Material/PBRToonShader.hlsli"
#include "../Material/ToonShader.hlsli"
#include "../Material/RimToonShader.hlsli"

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
    else if (input.materialID == 3)
    {
        Result = ShadeMaterial_Toon(input);
    }
    else if (input.materialID == 4)
    {
        Result = ShadeMaterial_RimToon(input);
    }
    else { /* default */ }

    return Result;
}
