#include "../common.hlsl"
#include "PostEffectFunc.hlsli"

// ライティング済みシーンテクスチャ (t0)
// g_SamplerState は PostEffectFunc.hlsli で定義済み
Texture2D g_Texture : register(t0);

// Parameter.x : フォーカス距離 (view-space 単位, 例: 5.0)
// Parameter.y : フォーカス範囲 (この距離内は完全シャープ, 例: 2.0)
// Parameter.z : 最大ブラー半径 (ピクセル数, 例: 8.0)

// Poisson ディスク サンプル (13 タップ、単位円内)
static const int   SAMPLE_COUNT = 13;
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

    float focusDist   = Parameter.x;
    float focusRange  = max(Parameter.y, 0.001f);
    float maxBlurPx   = max(Parameter.z, 0.0f);

    // テクスチャ解像度
    uint texWidth, texHeight;
    g_Texture.GetDimensions(texWidth, texHeight);
    float2 texelSize = float2(1.0f / texWidth, 1.0f / texHeight);

    // 線形深度取得 (view-space)
    float viewDepth = GetLinearDepth(uv);

    // CoC : フォーカス距離からの乖離で決まるブラー量 (0 = シャープ, 1 = 最大ブラー)
    // 注意: 中心ピクセルの CoC を全タップに使う実装のため、前景エッジが後景に
    //       わずかに滲む場合があります (単純シャープ／ブラー切り替え用途向け)。
    float coc        = saturate(abs(viewDepth - focusDist) / focusRange);
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
