#include "../common.hlsl"
#include "PostEffectFunc.hlsli"

// ============================================================
// Water SSR (Screen Space Reflections) Post-Effect
//
// WaveComponent の水面ピクセルに対して SSR を適用し、
// SSR が外れた場合はプロシージャルな空環境色にフォールバックする。
//
// Parameter.x : SSR ステップサイズ       (推奨値: 0.3)
// Parameter.y : 反射強度                 (推奨値: 0.6)
// Parameter.z : SSR 最大イテレーション数 (推奨値: 32)
// Parameter.w : 水面の ShaderID          (水面マテリアルの ID と一致させる)
// ============================================================

Texture2D g_Texture : register(t0);

#define SSR_MAX_STEPS 64

void main(in PS_IN In, out float4 outDiffuse : SV_Target)
{
    float2 uv = In.TexCoord;
    float4 sceneColor = g_Texture.Sample(g_SamplerState, uv);

    // 深度チェック：スカイボックス・未書き込みピクセルをスキップ
    float depth = GetDepth(uv);
    if (depth >= 1.0f)
    {
        outDiffuse = sceneColor;
        return;
    }

    // ShaderID で水面ピクセルを識別
    uint texWidth, texHeight;
    g_Texture.GetDimensions(texWidth, texHeight);
    int2 pixelCoord = int2(uv.x * texWidth, uv.y * texHeight);
    uint4 param = GetObjParam(pixelCoord);
    uint shaderID = param.z;
    int waterShaderID = (int) Parameter.w;

    if ((int) shaderID != waterShaderID)
    {
        outDiffuse = sceneColor;
        return;
    }

    // ---- 水面ピクセル: SSR 実行 ----
    float3 worldPos = GetWorldPosition(uv);
    float3 worldNormal = normalize(GetWorldNormal(uv));
    float3 viewDir = normalize(CameraPosition.xyz - worldPos);
    float3 reflDir = normalize(reflect(-viewDir, worldNormal));

    float stepSize = max(Parameter.x, 0.05f);
    int maxSteps = clamp((int) Parameter.z, 8, SSR_MAX_STEPS);
    float intensity = saturate(Parameter.y);

    // SSR レイマーチ
    float4 reflColor = float4(0, 0, 0, 0);
    bool hit = false;

    [loop]
    for (int i = 1; i <= maxSteps; i++)
    {
        float3 sampleWorldPos = worldPos + reflDir * stepSize * (float) i;

        // ワールド → クリップ空間
        float4 viewPos4 = mul(float4(sampleWorldPos, 1.0f), View);
        float4 clipPos = mul(viewPos4, Projection);

        if (clipPos.w <= 0.0f)
            break;

        clipPos.xyz /= clipPos.w;

        float2 sampleUV = float2(
             clipPos.x * 0.5f + 0.5f,
            -clipPos.y * 0.5f + 0.5f
        );

        // スクリーン外のサンプルは中断
        if (sampleUV.x < 0.0f || sampleUV.x > 1.0f ||
            sampleUV.y < 0.0f || sampleUV.y > 1.0f)
            break;

        // 深度比較
        float sampleDepth = GetLinearDepth(sampleUV);
        float rayDepth = viewPos4.z;

        float depthDiff = rayDepth - sampleDepth;

        if (depthDiff > 0.0f && depthDiff < stepSize * 2.0f)
        {
            // ヒット: 反射されたピクセルの色を取得
            reflColor = g_Texture.Sample(g_SamplerState, sampleUV);
            hit = true;
            break;
        }
    }

    // Fresnel 計算
    float NdotV = saturate(dot(worldNormal, viewDir));
    float fresnel = 0.02f + 0.98f * pow(1.0f - NdotV, 5.0f);

    if (hit)
    {
        // SSR ヒット: 反射色をブレンド
        outDiffuse = lerp(sceneColor, reflColor, fresnel * intensity);
    }
    else
    {
        // SSR ミス: プロシージャルスカイ環境でフォールバック
        float3 R = reflDir;

        // 空のグラデーション (上=青空, 水平線=淡い色, 下=暗い海色)
        float skyBlend = saturate(R.y);
        float3 skyColor = lerp(
            float3(0.6f, 0.7f, 0.8f),  // 水平線
            float3(0.3f, 0.5f, 0.9f),  // 天頂
            skyBlend
        );

        float3 envColor = skyColor;

        outDiffuse = lerp(sceneColor, float4(envColor, 1.0f), fresnel * intensity * 0.5f);
    }
}
