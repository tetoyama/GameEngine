#include "../common.hlsl"
#include "PostEffectFunc.hlsli"

// ライティング済みシーンテクスチャ (t0)
// g_SamplerState は PostEffectFunc.hlsli で定義済み
Texture2D g_Texture : register(t0);

// Parameter.x : エッジ感度 (深度差分の閾値スケール, 推奨値 1.0 〜 10.0)
// Parameter.y : エッジ幅ピクセル数 (推奨値 1.0)
//   深度バッファ (GBuffer t14) の隣接ピクセル差分でエッジを検出する。
//   ポストプロセスが GBuffer (深度) にアクセスできることを検証するシェーダー。

void main(in PS_IN In, out float4 outDiffuse : SV_Target)
{
    float2 uv = In.TexCoord;

    // テクスチャ解像度を取得してテクセルサイズを算出
    uint texWidth, texHeight;
    g_Texture.GetDimensions(texWidth, texHeight);
    float2 texelSize = float2(1.0f / texWidth, 1.0f / texHeight);

    float edgeWidth = max(Parameter.y, 1.0f);
    float2 off = texelSize * edgeWidth;

    // ライティング済みシーン色
    float4 sceneColor = g_Texture.Sample(g_SamplerState, uv);

    // GBuffer 深度を使ったエッジ検出 (Roberts cross 演算子)
    // 斜め方向の2ペアの深度差を組み合わせてエッジ強度を算出
    float d00 = GetLinearDepth(uv + float2(-off.x, -off.y));
    float d10 = GetLinearDepth(uv + float2( off.x, -off.y));
    float d01 = GetLinearDepth(uv + float2(-off.x,  off.y));
    float d11 = GetLinearDepth(uv + float2( off.x,  off.y));

    float gx = d11 - d00;
    float gy = d01 - d10;
    float edge = sqrt(gx * gx + gy * gy);
    edge = saturate(edge * Parameter.x);

    // エッジ部分を黒くする（縁取り）
    outDiffuse = lerp(sceneColor, float4(0, 0, 0, sceneColor.a), edge);
}
