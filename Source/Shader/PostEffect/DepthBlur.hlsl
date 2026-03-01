#include "../common.hlsl"
#include "PostEffectFunc.hlsli"

// ライティング済みシーンテクスチャ (t0)
// g_SamplerState は PostEffectFunc.hlsli で定義済み
Texture2D g_Texture : register(t0);

// Parameter.x : ブラーの強さ (最大ブラー半径, ピクセル数, 例: 8.0)
//   フォーカス距離は画面中央ピクセルの深度から自動取得。
//   他のパラメータ (y, z, w) は未使用。

// Poisson ディスク サンプル (13 タップ、単位円内)
static const int    SAMPLE_COUNT = 13;
static const float2 KERNEL[13] = {
    float2( 0.000f,  0.000f),
    float2( 1.000f,  0.000f),
    float2(-1.000f,  0.000f),
    float2( 0.000f,  1.000f),
    float2( 0.000f, -1.000f),
    float2( 0.707f,  0.707f),
    float2(-0.707f,  0.707f),
    float2( 0.707f, -0.707f),
    float2(-0.707f, -0.707f),
    float2( 0.500f,  0.866f),
    float2(-0.500f,  0.866f),
    float2( 0.500f, -0.866f),
    float2(-0.500f, -0.866f),
};

void main(in PS_IN In, out float4 outDiffuse : SV_Target)
{
    float2 uv = In.TexCoord;

    float maxBlurPx = max(Parameter.x, 0.0f);

    // テクスチャ解像度
    uint texWidth, texHeight;
    g_Texture.GetDimensions(texWidth, texHeight);
    float2 texelSize = float2(1.0f / texWidth, 1.0f / texHeight);

    // フォーカス距離 = 画面中央ピクセルの線形深度 (自動取得)
    float focusDist = GetLinearDepth(float2(0.5f, 0.5f));
    focusDist = max(focusDist, 0.001f);

    // 線形深度取得 (view-space)
    float viewDepth = GetLinearDepth(uv);

    // CoC : フォーカス距離からの乖離をフォーカス距離自体で正規化 (0=シャープ, 1=最大ブラー)
    // 注意: 中心ピクセルの CoC を全タップに使う実装のため、前景エッジが後景に
    //       わずかに滲む場合があります (単純シャープ／ブラー切り替え用途向け)。
    float coc        = saturate(abs(viewDepth - focusDist) / focusDist);
    float blurRadius = coc * maxBlurPx;

    // ブラー不要ならそのまま返す
    if (blurRadius < 0.5f)
    {
        outDiffuse = g_Texture.Sample(g_SamplerState, uv);
        return;
    }

    // Poisson ディスク ブラー
    float4 color = float4(0, 0, 0, 0);
    for (int i = 0; i < SAMPLE_COUNT; ++i)
    {
        float2 sampleUV = clamp(uv + KERNEL[i] * blurRadius * texelSize, 0.0f, 1.0f);
        color += g_Texture.Sample(g_SamplerState, sampleUV);
    }
    outDiffuse = color * (1.0f / SAMPLE_COUNT);
}
