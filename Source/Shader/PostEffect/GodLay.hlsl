#include "../common.hlsl"
#include "PostEffectFunc.hlsli"

// ============================================================
// GodLay (God Ray / ゴッドレイ + Light Shaft / ライトシャフト) Pixel Shader
//
// スクリーン空間で全ライトの光源位置に向けてラジアルブラーを適用し、
// エミッシブ・輝度の高い領域から放射する光芒（光条）と、
// 深度バッファを利用した体積的なライトシャフトを合成して出力する。
//
// 参考アルゴリズム:
//   Nvidia "Volumetric Light Scattering as a Post-Process" (2007)
//   by Kenny Mitchell / Sean O'Neil
//
// 全有効ライトを処理し、各ライトの寄与を加算合成する。
//   - 方向ライト (Directional / DirectionalCSM): Direction の逆方向の遠距離点を投影
//   - 点光源 / スポットライト (Point / Spot): ワールド座標を直接投影
//   - カメラ背後のライトは自動的にスキップ
//
// Parameter.x : ライトシャフト強度 (0 = 無効, 推奨値: 0.1 〜 0.5)
// Parameter.y : (予約)
// Parameter.z : ゴッドレイ強度 (Exposure)           (推奨値: 0.5 〜 2.0)
// Parameter.w : 減衰率 (Decay) [0.8, 1.0]           (推奨値: 0.97)
// ============================================================

// ライティング済みシーンテクスチャ (t0)
Texture2D g_Texture : register(t0);

// サンプリング数 (固定値: 多いほど高品質だが重い)
#define GODLAY_SAMPLE_COUNT 64

// 方向ライトをスクリーン投影するときの「仮想光源距離」(ワールド単位)
#define DIRECTIONAL_LIGHT_DISTANCE 1e5f

// ライトシャフト判定: この深度値以上はスカイ/背景とみなし、シャフトを通す
#define SHAFT_SKY_DEPTH_THRESHOLD 0.9999f

// エフェクト強度のゼロ判定しきい値 (これ未満は無効とみなしてスキップ)
#define MIN_EFFECT_THRESHOLD 1e-4f

// ワールド空間座標をスクリーン UV [0,1] に変換する
// 戻り値: (uv.x, uv.y, clip.w)  — clip.w > 0 のときのみ有効 (カメラ前方)
float3 WorldToScreenUVWithDepth(float3 worldPos)
{
    float4 clip = mul(float4(worldPos, 1.0f), View);
    clip        = mul(clip, Projection);
    float2 uv = float2(
         clip.x / clip.w * 0.5f + 0.5f,
        -clip.y / clip.w * 0.5f + 0.5f
    );
    return float3(uv, clip.w);
}

// LIGHT 構造体からスクリーン UV を取得する
// 戻り値: (uv.x, uv.y, clip.w)  — clip.w <= 0 の場合は無効
float3 GetLightScreenUV(LIGHT light)
{
    // 方向ライト (通常 / CSM カスケード先頭): Direction の逆方向が「太陽」の向き
    if (light.LightType == LIGHT_TYPE_DIRECTIONAL)
    {
        float3 sunDir      = -normalize(light.Direction.xyz);
        float3 sunWorldPos = CameraPosition.xyz + sunDir * DIRECTIONAL_LIGHT_DISTANCE;
        return WorldToScreenUVWithDepth(sunWorldPos);
    }

    if (light.LightType == LIGHT_TYPE_POINT ||
        light.LightType == LIGHT_TYPE_SPOT)
    {
        return WorldToScreenUVWithDepth(light.Position.xyz);
    }

    return float3(0.0f, 0.0f, -1.0f); // 無効
}

// ひとつのライトに対するゴッドレイ + ライトシャフト寄与を計算する
// lightScreenPos : ライトのスクリーン UV [0,1]
// lightColor     : ライトシャフトに使用するライトの拡散色
// exposure       : ゴッドレイ強度スケール
// decay          : サンプルごとの減衰率
// shaftIntensity : ライトシャフト強度 (0 = 無効)
float3 ComputeLightContrib(float2 uv, float2 lightScreenPos, float3 lightColor,
                           float exposure, float decay, float shaftIntensity)
{
    float2 delta = (lightScreenPos - uv) / float(GODLAY_SAMPLE_COUNT);

    float  illuminationDecay = 1.0f;
    float3 accum             = float3(0.0f, 0.0f, 0.0f);
    float2 sampleUV          = uv;

    [loop]
    for (int i = 0; i < GODLAY_SAMPLE_COUNT; i++)
    {
        sampleUV += delta;
        float2 clampedUV = saturate(sampleUV);

        // --- ゴッドレイ: エミッシブ + 高輝度成分 ---
        float4 emissive        = GetEmissive(clampedUV);
        float3 emissiveContrib = emissive.rgb * emissive.a;

        float4 s   = g_Texture.Sample(g_SamplerState, clampedUV);
        float  lum = dot(s.rgb, float3(0.2126f, 0.7152f, 0.0722f));
        float3 brightContrib = s.rgb * max(lum - 0.7f, 0.0f);

        // --- ライトシャフト: 深度バッファでスカイ/開放空間を検出 ---
        float3 shaftContrib = float3(0.0f, 0.0f, 0.0f);
        if (shaftIntensity >= MIN_EFFECT_THRESHOLD)
        {
            float depth  = GetDepth(clampedUV);
            shaftContrib = lightColor * shaftIntensity
                           * (depth >= SHAFT_SKY_DEPTH_THRESHOLD ? 1.0f : 0.0f);
        }

        accum += (emissiveContrib + brightContrib + shaftContrib) * illuminationDecay;
        illuminationDecay *= decay;
    }

    return accum * exposure / float(GODLAY_SAMPLE_COUNT);
}

void main(in PS_IN In, out float4 outDiffuse : SV_Target)
{
    float2 uv = In.TexCoord;

    // シーンカラー取得
    float4 sceneColor = g_Texture.Sample(g_SamplerState, uv);

    // パラメータ取得
    float shaftIntensity = max(Parameter.x, 0.0f);
    float exposure       = max(Parameter.z, 0.0f);
    float decay          = clamp(Parameter.w, 0.0f, 1.0f);

    // ゼロ付近の強度ではゴッドレイ/シャフト処理をスキップ
    if (exposure < MIN_EFFECT_THRESHOLD && shaftIntensity < MIN_EFFECT_THRESHOLD)
    {
        outDiffuse = sceneColor;
        return;
    }

    // ----------------------------------------------------------------
    // 全有効ライトの寄与を積算
    // ----------------------------------------------------------------
    float3 totalContrib = float3(0.0f, 0.0f, 0.0f);

    [loop]
    for (int li = 0; li < ActiveLightCount; li++)
    {
        LIGHT light = Lights[li];
        if (!light.Enable) continue;

        // CSM カスケードの 2 番目以降エントリはスキップ (重複処理回避)
        if (light.Dummy >= 2) continue;

        // ライトのスクリーン UV を取得 (カメラ背後は clip.w <= 0 でスキップ)
        float3 screenResult = GetLightScreenUV(light);
        if (screenResult.z <= 0.0f) continue;

        totalContrib += ComputeLightContrib(
            uv, screenResult.xy, light.Diffuse.rgb,
            exposure, decay, shaftIntensity
        );
    }
    // ----------------------------------------------------------------

    // シーンカラーにゴッドレイ + ライトシャフトを加算合成
    outDiffuse = float4(sceneColor.rgb + totalContrib, sceneColor.a);
}
