#include "../common.hlsl"
#include "PostEffectFunc.hlsli"

// ライティング済みシーンテクスチャ (t0: ノードエディタでメイン入力に接続)
// g_SamplerState は PostEffectFunc.hlsli で定義済みのため再宣言不要
Texture2D g_Texture : register(t0);

// Parameter.x : エッジ強度 (推奨値 3.0 〜 10.0、デフォルト 5.0)
// Parameter.y : エッジ幅ピクセル数 (推奨値 1.0)

void main(in PS_IN In, out float4 outDiffuse : SV_Target)
{
    float2 uv = In.TexCoord;

    // テクスチャ解像度を取得してテクセルサイズを算出
    uint texWidth, texHeight;
    g_Texture.GetDimensions(texWidth, texHeight);
    float2 texelSize = float2(1.0f / texWidth, 1.0f / texHeight);

    float edgeWidth = max(Parameter.y, 1.0f);

    // ライティング済みシーン色
    float4 sceneColor = g_Texture.Sample(g_SamplerState, uv);

    // GBuffer の法線を使ったエッジ検出
    // 隣接ピクセルとの法線差分が大きい箇所をエッジとみなす
    float3 n  = GetWorldNormal(uv);
    float3 nR = GetWorldNormal(uv + float2(texelSize.x * edgeWidth, 0));
    float3 nU = GetWorldNormal(uv + float2(0, texelSize.y * edgeWidth));

    float edge = length(n - nR) + length(n - nU);
    edge = saturate(edge * Parameter.x);

    // エッジ部分を黒くする（縁取り）
    outDiffuse = lerp(sceneColor, float4(0, 0, 0, sceneColor.a), edge);
}
