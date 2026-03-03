// =============================================================
// SSR.hlsl  (Screen Space Reflections)
//
// GBuffer の金属度・法線・ワールド座標を元に、
// スクリーンスペースでレイマーチングを行い反射色を合成する。
// 金属度が高いピクセル (水面など) にリフレクションを適用する。
//
// 入力テクスチャ:
//   t0              : シーンカラー (ライティング済み)
//   t8-t14          : GBuffer (PostEffectFunc.hlsli 参照)
//
// Parameter (CbPerObject.Parameter):
//   x : ReflectionStrength  反射強度        (推奨: 0.8)
//   y : MetallicThreshold   金属度閾値      (推奨: 0.5)
//   z : MaxSteps            レイマーチ最大ステップ数 (推奨: 32)
//   w : MaxDistance         レイマーチ最大距離      (推奨: 5.0)
// =============================================================

#include "../common.hlsl"
#include "PostEffectFunc.hlsli"

Texture2D g_Texture : register(t0);

void main(in PS_IN In, out float4 outDiffuse : SV_Target)
{
    float2 uv = In.TexCoord;

    // シーンカラー取得
    float4 sceneColor = g_Texture.Sample(g_SamplerState, uv);

    // 未書き込みピクセル (スカイ等) はそのまま出力
    float depth = GetDepth(uv);
    if (depth >= 1.0f)
    {
        outDiffuse = sceneColor;
        return;
    }

    // パラメータ取得
    float reflStrength   = max(Parameter.x, 0.0f);
    float metallicThresh = saturate(Parameter.y);
    int   maxSteps       = max((int)Parameter.z, 4);
    float maxDist        = max(Parameter.w, 0.01f);

    // 金属度チェック (閾値未満はスキップ)
    float4 mat     = GetMaterial(uv);
    float  metallic = mat.r;
    if (metallic < metallicThresh)
    {
        outDiffuse = sceneColor;
        return;
    }

    // GBuffer からワールド空間の位置・法線を取得
    float3 worldPos    = GetWorldPosition(uv);
    float3 worldNormal = normalize(GetWorldNormal(uv));
    float3 viewDir     = normalize(CameraPosition.xyz - worldPos);

    // 反射方向 (ワールド空間)
    float3 reflDir = reflect(-viewDir, worldNormal);

    // ビュー空間に変換
    float3 viewPos  = mul(float4(worldPos,  1.0f), View).xyz;
    float3 viewRefl = normalize(mul(float4(reflDir, 0.0f), View).xyz);

    // レイマーチ
    float stepSize = maxDist / float(maxSteps);
    float2 hitUV   = uv;
    bool   hitFound = false;
    float  edgeFade = 0.0f;

    [loop]
    for (int i = 1; i <= maxSteps; i++)
    {
        float3 sampleVS = viewPos + viewRefl * stepSize * float(i);

        // ビュー空間 → クリップ空間 → NDC → UV
        float4 sampleClip = mul(float4(sampleVS, 1.0f), Projection);
        if (sampleClip.w <= 0.0f) break;
        sampleClip.xyz /= sampleClip.w;

        float2 sampleUV = float2(
             sampleClip.x *  0.5f + 0.5f,
            -sampleClip.y *  0.5f + 0.5f
        );

        // スクリーン外はループ終了
        if (any(sampleUV < 0.01f) || any(sampleUV > 0.99f)) break;

        // 深度未書き込みピクセルはスキップ
        float sampleDepth = GetDepth(sampleUV);
        if (sampleDepth >= 1.0f) continue;

        // GBuffer からサンプル点のビュー空間深度を取得
        float3 sampleWorldPos = GetWorldPosition(sampleUV);
        float3 sampleViewPos  = mul(float4(sampleWorldPos, 1.0f), View).xyz;

        // レイがサーフェス背面へ到達したらヒット
        float rayZ     = sampleVS.z;
        float surfaceZ = sampleViewPos.z;

        if (rayZ > surfaceZ && (rayZ - surfaceZ) < stepSize * 2.5f)
        {
            hitUV = sampleUV;

            // 画面端フェード (端に近いほど反射を弱める)
            float2 fade = smoothstep(0.0f, 0.1f, sampleUV)
                        * smoothstep(1.0f, 0.9f, sampleUV);
            edgeFade = fade.x * fade.y;

            hitFound = true;
            break;
        }
    }

    if (!hitFound)
    {
        outDiffuse = sceneColor;
        return;
    }

    // 反射色サンプリング
    float4 reflColor = g_Texture.Sample(g_SamplerState, hitUV);

    // 金属度・Fresnel によるブレンド係数
    float metallicFactor = smoothstep(metallicThresh, 1.0f, metallic);
    float blendFactor    = reflStrength * metallicFactor * edgeFade;

    outDiffuse = lerp(sceneColor, reflColor, saturate(blendFactor));
}
