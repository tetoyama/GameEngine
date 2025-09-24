#include "../common.hlsl"

Texture2D g_Texture : register(t0);
SamplerState g_SamplerState : register(s0);

void main(in PS_IN In, out float4 outDiffuse : SV_Target)
{
    // テクスチャをサンプリング
    float4 color = g_Texture.Sample(g_SamplerState, In.TexCoord);

    // 輝度を計算
    float luminance = dot(color.rgb, float3(0.299, 0.587, 0.114));

    // Parameter.x でグレースケール強度を調整（0 = カラー, 1 = 完全グレースケール）
    float3 finalColor = lerp(color.rgb, float3(luminance, luminance, luminance), Parameter.x);

    outDiffuse = float4(finalColor, color.a);
}
