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
// 光源のスクリーン空間座標は CbPerFrame の Lights[] から自動計算する。
//   - 優先順位: DirectionalCSM / Directional → 光の方向を逆算してスクリーン投影
//   - フォールバック: Point / Spot → ワールド座標をスクリーン投影
//   - 有効なライトが見つからない場合は画面中央 (0.5, 0.5) を使用
//
// Parameter.x : (未使用 — 旧: 手動指定の光源スクリーン X)
// Parameter.y : (未使用 — 旧: 手動指定の光源スクリーン Y)
// Parameter.z : ゴッドレイ強度 (Exposure)           (推奨値: 0.5 〜 2.0)
// Parameter.w : 減衰率 (Decay) [0.8, 1.0]           (推奨値: 0.97)
// ============================================================

// ライティング済みシーンテクスチャ (t0)
Texture2D g_Texture : register(t0);

// サンプリング数 (固定値: 多いほど高品質だが重い)
#define GODLAY_SAMPLE_COUNT 64

// 方向ライトをスクリーン投影するときの「仮想光源距離」(ワールド単位)
// 十分遠い点として投影することで方向ライトのスクリーン UV を求める
#define DIRECTIONAL_LIGHT_DISTANCE 1e5f

// ワールド空間座標をスクリーン UV [0,1] に変換する
// 戻り値: (uv.x, uv.y, clip.w)  — clip.w > 0 のときのみ有効 (カメラ前方)
float3 WorldToScreenUVWithDepth(float3 worldPos)
{
    float4 clip = mul(float4(worldPos, 1.0f), View);
    clip        = mul(clip, Projection);
    // w で除算して NDC → UV 変換 (y 軸反転: DirectX の UV 原点は左上)
    float2 uv = float2(
         clip.x / clip.w * 0.5f + 0.5f,
        -clip.y / clip.w * 0.5f + 0.5f
    );
    return float3(uv, clip.w); // z に clip.w を返して有効性チェックに使う
}

void main(in PS_IN In, out float4 outDiffuse : SV_Target)
{
    float2 uv = In.TexCoord;

    // シーンカラー取得
    float4 sceneColor = g_Texture.Sample(g_SamplerState, uv);

    // パラメータ取得
    float  exposure  = max(Parameter.z, 0.0f);
    float  decay     = clamp(Parameter.w, 0.0f, 1.0f);

    // ----------------------------------------------------------------
    // ライトのコンスタントバッファから光源スクリーン座標を自動計算
    // ----------------------------------------------------------------
    float2 lightPos = float2(0.5f, 0.5f); // デフォルト: 画面中央
    bool   found    = false;

    // パス 1: 方向ライト (DirectionalCSM / Directional) を優先検索
    [loop]
    for (int li = 0; li < ActiveLightCount; li++)
    {
        LIGHT light = Lights[li];
        if (!light.Enable) continue;
        if (light.LightType != LIGHT_TYPE_DIRECTIONAL &&
            light.LightType != LIGHT_TYPE_DIRECTIONAL_CSM) continue;

        // 方向ライトの場合: Direction の逆方向が「太陽」の向き
        // カメラ位置から十分遠い点に投影してスクリーン UV を求める
        float3 sunDir      = -normalize(light.Direction.xyz);
        float3 sunWorldPos = CameraPosition.xyz + sunDir * DIRECTIONAL_LIGHT_DISTANCE;
        float3 result      = WorldToScreenUVWithDepth(sunWorldPos);

        if (result.z > 0.0f) // カメラ前方にある場合のみ有効
        {
            lightPos = result.xy;
            found    = true;
            break;
        }
    }

    // パス 2: 方向ライトが見つからなければ点光源・スポットライトを検索
    if (!found)
    {
        [loop]
        for (int li2 = 0; li2 < ActiveLightCount; li2++)
        {
            LIGHT light2 = Lights[li2];
            if (!light2.Enable) continue;
            if (light2.LightType != LIGHT_TYPE_POINT &&
                light2.LightType != LIGHT_TYPE_SPOT) continue;

            float3 result = WorldToScreenUVWithDepth(light2.Position.xyz);

            if (result.z > 0.0f)
            {
                lightPos = result.xy;
                found    = true;
                break;
            }
        }
    }
    // ----------------------------------------------------------------

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
