#include "../common.hlsl"
#include "PostEffectFunc.hlsli"

// ============================================================
// SSR (Screen Space Reflections) Pixel Shader
//
// GBuffer のワールド座標・法線からビュー空間で反射レイをマーチし、
// スクリーン上で交差したピクセルの色を反射色として合成する。
// フレネル係数とスクリーン端フェードにより自然な反射を実現する。
//
// Parameter.x : レイマーチ最大ステップ数 (推奨値: 32)
// Parameter.y : 1ステップの移動量 (ビュー空間, 推奨値: 0.2)
// Parameter.z : 交差判定の厚みしきい値 (推奨値: 0.3)
// ============================================================

// ライティング済みシーンテクスチャ (t0)
Texture2D g_Texture : register(t0);

void main(in PS_IN In, out float4 outDiffuse : SV_Target)
{
    float2 uv = In.TexCoord;

    // シーンカラー取得
    float4 sceneColor = g_Texture.Sample(g_SamplerState, uv);

    // スカイ・未書き込みピクセルは SSR をスキップ
    float ndcDepth = GetDepth(uv);
    if (ndcDepth >= 1.0f)
    {
        outDiffuse = sceneColor;
        return;
    }

    // GBuffer からワールド空間の情報を取得
    float3 worldPos = GetWorldPosition(uv);
    float3 worldNormal = normalize(GetWorldNormal(uv));
    float3 viewDir = normalize(CameraPosition.xyz - worldPos);

    // フレネル係数: 視線と法線の角度が大きいほど反射が強い（Schlick 近似, 指数 4.0）
    // ※ 純粋な Schlick は指数 5.0 だが, 4.0 はエンジン既存シェーダーとの統一値
    float fresnel = pow(1.0f - saturate(dot(worldNormal, viewDir)), 4.0f) * 0.98f + 0.02f;

    // ビュー空間へ変換
    float3 viewPos = mul(float4(worldPos, 1.0f), View).xyz;
    float3 viewNormal = normalize(mul(float4(worldNormal, 0.0f), View).xyz);
    float3 viewDirVS = normalize(mul(float4(viewDir, 0.0f), View).xyz);

    // ビュー空間の反射方向
    float3 reflDir = normalize(reflect(-viewDirVS, viewNormal));

    // パラメータ取得
    int maxSteps = (int) clamp(Parameter.x, 4.0f, 64.0f);
    float stepSize = max(Parameter.y, 0.001f);
    float reflStrength = saturate(GetMaterial(In.TexCoord).x);
    float thickness = max(Parameter.z, 0.01f);

    // ---- レイマーチ ----
    float3 currentPos = viewPos;
    float2 hitUV = (float2) 0;
    bool hit = false;

    [loop]
    for (int i = 0; i < maxSteps; i++)
    {
        // 加速ステップ: t が 0→1 に増えるにつれて移動量が stepSize × 1.0 〜 3.0 倍に増える
        // (近くは細かく判定、遠くは大きく進む)
        float t = (float) (i + 1) / (float) maxSteps;
        currentPos += reflDir * stepSize * (1.0f + t * 2.0f);

        // ビュー空間 → クリップ空間 → NDC → UV
        float4 clipPos = mul(float4(currentPos, 1.0f), Projection);
        if (clipPos.w <= 0.0f)
            break;
        clipPos.xyz /= clipPos.w;

        float2 sampleUV = float2(
             clipPos.x * 0.5f + 0.5f,
            -clipPos.y * 0.5f + 0.5f
        );

        // スクリーン外は打ち切り
        if (sampleUV.x < 0.0f || sampleUV.x > 1.0f ||
            sampleUV.y < 0.0f || sampleUV.y > 1.0f)
            break;

        // GBuffer からサンプル位置のビュー空間 Z 深度を取得
        float3 sampleWorldPos = GetWorldPosition(sampleUV);
        float sampleViewZ = mul(float4(sampleWorldPos, 1.0f), View).z;

        // 深度差で交差判定
        // currentPos.z > sampleViewZ : レイが GBuffer 表面の後ろに回った
        float depthDiff = currentPos.z - sampleViewZ;
        if (depthDiff > 0.0f && depthDiff < thickness)
        {
            hitUV = sampleUV;
            hit = true;
            break;
        }
    }

    if (hit)
    {
        float4 reflColor = g_Texture.Sample(g_SamplerState, hitUV);

        // スクリーン端フェードアウト: 端に近いほど反射を薄くする
        float2 edgeFade2 = min(hitUV, 1.0f - hitUV) * 5.0f;
        float edgeFade = saturate(min(edgeFade2.x, edgeFade2.y));

        float blendFactor = fresnel * reflStrength * edgeFade;
        outDiffuse = lerp(sceneColor, reflColor, blendFactor);
    }
    else
    {
        outDiffuse = sceneColor;
    }
}