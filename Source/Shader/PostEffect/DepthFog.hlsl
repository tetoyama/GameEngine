#include "../common.hlsl"
#include "PostEffectFunc.hlsli"

// ライティング済みシーンテクスチャ (t0)
// g_SamplerState は PostEffectFunc.hlsli で定義済み
Texture2D g_Texture : register(t0);

// Parameter.x : フォグ開始距離   (ビュー空間, 例: 10.0)
// Parameter.y : フォグ終了距離   (ビュー空間, 例: 100.0)
// Parameter.z : フォグ密度       (0.0 〜 1.0, 例: 1.0)
// Parameter.w : フォグの指数     (1.0 = 線形, 2.0 = 二乗で収束, 例: 1.0)
//
// フォグカラーはシェーダー内固定 (ライトグレー: RGB 0.7, 0.75, 0.8)
// 距離に応じてシーンカラーとフォグカラーをブレンドする。
// GBuffer の深度 (t14) から view-space の線形距離を算出して使用する。

// フォグカラー (ライトグレー)
static const float3 FOG_COLOR      = float3(0.7f, 0.75f, 0.8f);
// ゼロ除算防止: フォグ範囲の最小幅
static const float  MIN_FOG_RANGE  = 0.001f;
// pow() の指数下限: 0 に近い値での数値不安定を防ぐ
static const float  MIN_FOG_EXPONENT = 0.1f;

void main(in PS_IN In, out float4 outDiffuse : SV_Target)
{
    float2 uv = In.TexCoord;

    // シーンカラー取得
    float4 sceneColor = g_Texture.Sample(g_SamplerState, uv);

    // スカイボックス・未書き込みピクセルはフォグ処理をスキップ
    float ndcDepth = GetDepth(uv);
    if (ndcDepth >= 1.0f)
    {
        outDiffuse = sceneColor;
        return;
    }

    // パラメータ取得
    float fogStart   = max(Parameter.x, 0.0f);
    float fogEnd     = max(Parameter.y, fogStart + MIN_FOG_RANGE);
    float fogDensity = saturate(Parameter.z);
    float fogExp     = max(Parameter.w, MIN_FOG_EXPONENT);

    // view-space 線形深度取得 (カメラからの距離)
    float viewDepth = GetLinearDepth(uv);

    // フォグ係数 (0 = フォグなし, 1 = 完全フォグ)
    float fogFactor = saturate((viewDepth - fogStart) / (fogEnd - fogStart));
    fogFactor       = pow(fogFactor, fogExp);
    fogFactor      *= fogDensity;

    // フォグカラーとシーンカラーをブレンド
    float3 finalColor = lerp(sceneColor.rgb, FOG_COLOR, fogFactor);
    outDiffuse = float4(finalColor, sceneColor.a);
}
