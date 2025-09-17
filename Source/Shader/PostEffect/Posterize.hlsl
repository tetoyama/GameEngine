#include "../common.hlsl"

Texture2D g_Texture : register(t0);
SamplerState g_SamplerState : register(s0);

// ポスタライズの階調数
static const float LEVELS = 4.0;

void main(in PS_IN In, out float4 outDiffuse : SV_Target)
{
    // テクスチャをサンプリング
    float4 color = g_Texture.Sample(g_SamplerState, In.TexCoord);

    // RGB各チャンネルをポスタライズ
    color.rgb = floor(color.rgb * LEVELS) / (LEVELS - 1);

    // 出力
    outDiffuse = color;
}
