#include "../common.hlsl"
#include "PostEffectFunc.hlsli"

// ============================================================
// LensFlare (レンズフレア) Pixel Shader
//
// スクリーン空間で各ライトの位置を中心にレンズフレアを生成する。
// フレアはゴースト (ゴーストスポット)、ハロ (輝きの輪)、
// 中心グロー (光源周辺のソフトグロー) から構成される。
//
// GodLay と同様に CbPerFrame の全有効ライトを処理し、
// 各ライトの寄与を加算合成する。
// カメラ背後のライトや深度バッファで遮蔽されたライトは自動スキップ。
//
// 方向ライト (Directional / DirectionalCSM):
//   Direction の逆方向の遠距離点を投影してスクリーン UV を求める。
//   スクリーン内に太陽位置がある場合のみ描画する。
// 点光源 / スポットライト (Point / Spot):
//   ワールド座標を直接投影。深度バッファで遮蔽チェックを行う。
//
// Parameter.x : フレア全体の強度 (推奨値: 0.5 〜 2.0)
// Parameter.y : ハロ強度         (推奨値: 0.0 〜 1.0)
// Parameter.z : ゴースト強度     (推奨値: 0.0 〜 1.0)
// Parameter.w : ゴースト分散     (推奨値: 0.2 〜 0.6)
// ============================================================

// ライティング済みシーンテクスチャ (t0)
Texture2D g_Texture : register(t0);

// ========================== 定数 ==========================

// ゴースト数 (1ライトあたり固定; [unroll] のために定数が必要)
#define FLARE_GHOST_COUNT 6

// ゴーストの軸方向オフセット: 0=ライト位置, 1=画面中央, 2=中央の反対側
// オフセット < 1.0: ライト〜画面中央の間, >= 1.0: 画面中央の反対側
// 値は光学的フレア現象 (実写レファレンス) をもとに審美的にチューニング
static const float GHOST_OFFSETS[FLARE_GHOST_COUNT] =
{
    0.30f, 0.55f, 0.80f, 1.10f, 1.45f, 1.80f
};

// ゴーストのサイズ (UV 空間; 縦方向を基準)
static const float GHOST_SIZES[FLARE_GHOST_COUNT] =
{
    0.14f, 0.09f, 0.07f, 0.11f, 0.06f, 0.08f
};

// ゴーストの色調 (RGB) — 色収差効果として各ゴーストに異なる色を付与
static const float3 GHOST_CHROMA[FLARE_GHOST_COUNT] =
{
    float3(1.00f, 0.90f, 0.80f),   // 暖色
    float3(0.85f, 0.90f, 1.00f),   // 寒色
    float3(0.90f, 1.00f, 0.85f),   // 緑系
    float3(1.00f, 0.80f, 0.70f),   // オレンジ
    float3(0.75f, 0.90f, 1.00f),   // 水色
    float3(1.00f, 1.00f, 0.80f),   // 黄色
};

// ハロ半径 (UV 空間; 縦方向を基準)
#define HALO_RADIUS    0.22f

// 中心グロー: 広がり係数 (値が小さいほど鋭く集中)
#define GLOW_SPREAD    0.20f

// 最小効果しきい値 (これ未満は無効)
#define MIN_FLARE_THRESHOLD 1e-4f

// 遮蔽チェック: この NDC 深度以上ならスカイ/背景とみなす
#define SKY_DEPTH_THRESHOLD    0.9999f

// 方向ライト投影距離
#define DIRECTIONAL_LIGHT_DISTANCE 1e5f

// 点光源/スポットの遮蔽判定バイアス (NDC 深度)
#define DEPTH_BIAS_OCCLUSION 0.005f

// スクリーン外フレアのマージン (UV 範囲外でもフレアが滲み出るための許容量)
#define SCREEN_MARGIN_MIN   (-0.2f)
#define SCREEN_MARGIN_MAX   ( 1.2f)

// ========================== ヘルパー ==========================

// ワールド座標 → スクリーン UV + clip.w
// 戻り値: (uv.x, uv.y, clip.w) — clip.w > 0 のときのみカメラ前方
float3 WorldToScreenUVWithDepth(float3 worldPos)
{
    float4 clip = mul(float4(worldPos, 1.0f), View);
    clip        = mul(clip, Projection);
    float2 uv   = float2(
         clip.x / clip.w * 0.5f + 0.5f,
        -clip.y / clip.w * 0.5f + 0.5f
    );
    return float3(uv, clip.w);
}

// LIGHT 構造体 → スクリーン UV + clip.w
float3 GetLightScreenUV(LIGHT light)
{
    if (light.LightType == LIGHT_TYPE_DIRECTIONAL ||
        light.LightType == LIGHT_TYPE_DIRECTIONAL_CSM)
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

// アスペクト補正付きのピクセル間 UV 距離を返す
// aspect = screenWidth / screenHeight
float UVDist(float2 a, float2 b, float aspect)
{
    float2 d = (a - b) * float2(aspect, 1.0f);
    return length(d);
}

// ソフトな円形マスク (中心からの距離が radius 以下で明るくなる)
float SoftDisc(float dist, float radius)
{
    float t = saturate(1.0f - dist / max(radius, 1e-5f));
    return t * t;  // 二乗で柔らかく
}

// ソフトなリング (中心から targetRadius 距離付近で明るくなる)
float SoftRing(float dist, float targetRadius, float width)
{
    float t = saturate(1.0f - abs(dist - targetRadius) / max(width, 1e-5f));
    return t * t;
}

// 1ライト分のレンズフレア寄与を計算する
float3 ComputeFlareContrib(
    float2 uv,             // 現在のピクセル UV
    float2 lightPos,       // ライトのスクリーン UV
    float3 lightColor,     // ライトの拡散色
    float  intensity,      // 全体強度
    float  haloIntensity,  // ハロ強度
    float  ghostIntensity, // ゴースト強度
    float  dispersal,      // ゴースト分散
    float  aspect,         // 画面アスペクト比
    float  occlusion       // 遮蔽係数 [0,1]
)
{
    float3 result = float3(0.0f, 0.0f, 0.0f);

    // ライトから画面中央へのベクトル (軸方向)
    float2 axis = float2(0.5f, 0.5f) - lightPos;

    // ---- 中心グロー (光源直近のソフトな発光) ----
    {
        float dist  = UVDist(uv, lightPos, aspect);
        float glow  = exp(-dist * dist / (GLOW_SPREAD * GLOW_SPREAD));
        result     += lightColor * glow;
    }

    // ---- ハロ (光源周辺のリング) ----
    {
        float dist = UVDist(uv, lightPos, aspect);
        float ring = SoftRing(dist, HALO_RADIUS, HALO_RADIUS * 0.30f);
        result    += lightColor * ring * haloIntensity;
    }

    // ---- ゴースト (フレア軸上のスポット群) ----
    [unroll]
    for (int i = 0; i < FLARE_GHOST_COUNT; i++)
    {
        float2 ghostCenter = lightPos + axis * (GHOST_OFFSETS[i] * dispersal);
        float  dist        = UVDist(uv, ghostCenter, aspect);
        float  disc        = SoftDisc(dist, GHOST_SIZES[i]);
        result += lightColor * GHOST_CHROMA[i] * disc * ghostIntensity;
    }

    return result * intensity * occlusion;
}

// ========================== main ==========================

void main(in PS_IN In, out float4 outDiffuse : SV_Target)
{
    float2 uv = In.TexCoord;

    // シーンカラー取得
    float4 sceneColor = g_Texture.Sample(g_SamplerState, uv);

    // パラメータ取得
    float intensity      = max(Parameter.x, 0.0f);
    float haloIntensity  = max(Parameter.y, 0.0f);
    float ghostIntensity = max(Parameter.z, 0.0f);
    float dispersal      = max(Parameter.w, 0.1f);

    // 強度がほぼゼロならスキップ
    if (intensity < MIN_FLARE_THRESHOLD)
    {
        outDiffuse = sceneColor;
        return;
    }

    // アスペクト比: Projection._m11 / Projection._m00
    // XMMatrixPerspectiveFovLH では m00 = cot(fov/2)/aspect, m11 = cot(fov/2)
    float aspect = Projection._m11 / max(Projection._m00, 1e-5f);

    // ----------------------------------------------------------------
    // 全有効ライトの寄与を積算
    // ----------------------------------------------------------------
    float3 totalFlare = float3(0.0f, 0.0f, 0.0f);

    [loop]
    for (int li = 0; li < ActiveLightCount; li++)
    {
        LIGHT light = Lights[li];
        if (!light.Enable) continue;

        // ライトのスクリーン UV を取得 (カメラ背後は clip.w <= 0 でスキップ)
        float3 screenResult = GetLightScreenUV(light);
        if (screenResult.z <= 0.0f) continue;

        float2 lightPos = screenResult.xy;

        // スクリーン外のライトはフレアなし (UV 範囲外)
        if (any(lightPos < SCREEN_MARGIN_MIN) || any(lightPos > SCREEN_MARGIN_MAX)) continue;

        // ---- 遮蔽チェック ----
        float occlusion = 1.0f;

        if (all(lightPos >= 0.0f) && all(lightPos <= 1.0f))
        {
            if (light.LightType == LIGHT_TYPE_DIRECTIONAL ||
                light.LightType == LIGHT_TYPE_DIRECTIONAL_CSM)
            {
                // 方向ライト: ライト位置の深度がスカイ (≈1.0) ならば可視
                float sceneDepth = GetDepth(lightPos);
                occlusion = (sceneDepth >= SKY_DEPTH_THRESHOLD) ? 1.0f : 0.0f;
            }
            else if (light.LightType == LIGHT_TYPE_POINT ||
                     light.LightType == LIGHT_TYPE_SPOT)
            {
                // 点光源/スポット: ライトの NDC 深度 vs シーン深度で遮蔽判定
                float sceneDepth = GetDepth(lightPos);
                float4 clipPos   = mul(float4(light.Position.xyz, 1.0f), View);
                clipPos          = mul(clipPos, Projection);
                float lightNDC   = clipPos.z / max(clipPos.w, 1e-5f);
                // lightNDC > sceneDepth + バイアス ならライトは遮蔽されている
                occlusion = (lightNDC <= sceneDepth + DEPTH_BIAS_OCCLUSION) ? 1.0f : 0.0f;
            }
        }

        if (occlusion < MIN_FLARE_THRESHOLD) continue;

        // ライトの輝度スケール (明るいライトほど強いフレア)
        float lightLum   = dot(light.Diffuse.rgb, float3(0.2126f, 0.7152f, 0.0722f));
        float lightScale = saturate(lightLum);

        totalFlare += ComputeFlareContrib(
            uv, lightPos, light.Diffuse.rgb,
            intensity * lightScale, haloIntensity, ghostIntensity, dispersal,
            aspect, occlusion
        );
    }
    // ----------------------------------------------------------------

    // シーンカラーにレンズフレアを加算合成
    outDiffuse = float4(sceneColor.rgb + totalFlare, sceneColor.a);
}
