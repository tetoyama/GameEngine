#include "../common.hlsl"
#include "PostEffectFunc.hlsli"

// ============================================================
// GodLay (God Ray / ゴッドレイ) Pixel Shader
//
// スクリーン空間で光源位置に向けてラジアルブラーを適用し、
// エミッシブ・輝度の高い領域から放射する光芒（光条）を生成する。
//
// 参考アルゴリズム:
//   Nvidia "Volumetric Light Scattering as a Post-Process" (2007)
//   by Kenny Mitchell / Sean O'Neil
//
// Parameter.x : 光源のスクリーン空間 X 座標 [0,1]  (デフォルト: 0.5 = 中央)
// Parameter.y : 光源のスクリーン空間 Y 座標 [0,1]  (デフォルト: 0.5 = 中央)
// Parameter.z : ゴッドレイ強度 (Exposure)           (推奨値: 0.5 〜 2.0)
// Parameter.w : 減衰率 (Decay) [0.8, 1.0]           (推奨値: 0.97)
// ============================================================

// ライティング済みシーンテクスチャ (t0)
Texture2D g_Texture : register(t0);

// サンプリング数 (固定値: 多いほど高品質だが重い)
#define GODLAY_SAMPLE_COUNT 64

void main(in PS_IN In, out float4 outDiffuse : SV_Target)
{
    float2 uv = In.TexCoord;

    // シーンカラー取得
    float4 sceneColor = g_Texture.Sample(g_SamplerState, uv);

    // パラメータ取得
    float2 lightPos  = float2(Parameter.x, Parameter.y);
    float  exposure  = max(Parameter.z, 0.0f);
    float  decay     = clamp(Parameter.w, 0.0f, 1.0f);

    // ゼロ付近の強度ではゴッドレイをスキップしてシーンカラーをそのまま返す
    if (exposure < 1e-4f)
    {
        outDiffuse = sceneColor;
        return;
    }

    // 現在のピクセルから光源へ向かうステップベクトル
    float2 delta = (lightPos - uv) / float(GODLAY_SAMPLE_COUNT);

    // ラジアルブラーによる光散乱積算
    float  illuminationDecay = 1.0f;
    float3 godRayAccum       = float3(0.0f, 0.0f, 0.0f);
    float2 sampleUV          = uv;

    // ラジアルブラーループ ([loop]: 大きいサンプル数のためループを維持)
    [loop]
    for (int i = 0; i < GODLAY_SAMPLE_COUNT; i++)
    {
        sampleUV += delta;

        // スクリーン外をクランプ (saturate は [0,1] クランプの GPU ネイティブ命令)
        float2 clampedUV = saturate(sampleUV);

        // エミッシブ成分を光源として利用
        float4 emissive = GetEmissive(clampedUV);
        float3 emissiveContrib = emissive.rgb * emissive.a;

        // シーンカラーから輝度の高い成分を抽出 (明るさ閾値 0.7 超え)
        float4 s = g_Texture.Sample(g_SamplerState, clampedUV);
        float  lum = dot(s.rgb, float3(0.2126f, 0.7152f, 0.0722f));
        float3 brightContrib = s.rgb * max(lum - 0.7f, 0.0f);

        // 光源への距離に応じた減衰を適用して積算
        godRayAccum += (emissiveContrib + brightContrib) * illuminationDecay;
        illuminationDecay *= decay;
    }

    // 正規化 (サンプル数で割って強度スケール適用)
    godRayAccum *= exposure / float(GODLAY_SAMPLE_COUNT);

    // シーンカラーにゴッドレイを加算合成
    outDiffuse = float4(sceneColor.rgb + godRayAccum, sceneColor.a);
}
