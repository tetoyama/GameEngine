#include "../commonDefine.h"
#include "../common.hlsl"
#include "../Material/MaterialDefine.hlsli"
#include "../Material/FowardFunc.hlsli"
#include "../Material/MaterialFunc.hlsli"
#include "../Material/PBRToonShader.hlsli"

float4 main(PS_IN In) : SV_Target
{
    MaterialInput input = GetMaterialInput(In);
    float4 Result = ShadeMaterial_PBRToon(input);

    if (Result.a <= ALPHA_CLIP_THRESHOLD)
    {
        discard;
    }

    return Result;
}
