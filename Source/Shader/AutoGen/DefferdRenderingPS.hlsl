#include "../commonDefine.h"
#include "../common.hlsl"

#include "../Material/MaterialDefine.hlsli"

#include "../Material/DeferredFunc.hlsli"

#include "../Material/MaterialFunc.hlsli"

// material
#include "../Material/UnlitShader.hlsli"
#include "../Material/PBRShader.hlsli"

float4 main(PS_IN In) : SV_Target
{
    MaterialInput input = GetMaterialInput(In);

    // デフォルト色（未対応 material 用）
    float4 Result = float4(1, 0, 1, 1);

    switch (input.materialID)
    {
        case 0:
            Result = ShadeMaterial_Unlit(input);
            break;
        case 1:
            Result = ShadeMaterial_PBR(input);
            break;

        default:
            break;
    }

    return Result;
}
