#include "common.hlsl"

void main(in PS_IN In, out float4 outDiffuse : SV_Target)
{
    outDiffuse = float4(0.0f, 0.0f, 0.0f, 1.0f); // アウトラインの色を設定)
}