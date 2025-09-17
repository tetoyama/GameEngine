#include "../common.hlsl"

Texture2D g_Texture : register(t0);
SamplerState g_SamplerState : register(s0);

// モザイクのブロックサイズ（0.0～1.0 の UV 空間）
static const float MosaicBlockSize = 0.05f; // 5% ごとにまとめる

void main(in PS_IN In, out float4 outDiffuse : SV_Target)
{
    // UV をブロック単位に丸める
    float2 uv = In.TexCoord;
    uv = floor(uv / MosaicBlockSize) * MosaicBlockSize + MosaicBlockSize * 0.5;

    // テクスチャをサンプル
    outDiffuse = g_Texture.Sample(g_SamplerState, uv);
}
