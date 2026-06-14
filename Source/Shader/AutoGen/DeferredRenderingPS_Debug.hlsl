#include "../commonDefine.h"
#include "../common.hlsl"
#include "../Material/MaterialDefine.hlsli"
#include "../Material/DeferredFunc.hlsli"
#include "../Material/MaterialFunc.hlsli"

float4 main(PS_IN In) : SV_Target
{
    if (GetMaterialID(In) < 5u)
    {
        discard;
    }
    return float4(1, 0, 1, 1);
}
