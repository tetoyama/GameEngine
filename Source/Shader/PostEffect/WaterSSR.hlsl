#include "../common.hlsl"
#include "PostEffectFunc.hlsli"

// ============================================================
// Water SSR (Screen Space Reflections) Post-Effect
//
// Parameter.w で指定した ShaderID のピクセルにのみ SSR を適用する。
// SSR が外れた場合はシーンカラーをそのまま出力する
// （環境反射は WaterSurfaceShader で既に適用済み）
//
// Parameter.x : SSR ステップサイズ       (推奨値: 1.0)
// Parameter.y : 反射強度                 (推奨値: 0.8)
// Parameter.z : SSR 最大イテレーション数 (推奨値: 48)
// Parameter.w : 対象の ShaderID          (0 以下ならデフォルト 5)
// ============================================================

Texture2D g_Texture : register(t0);

#define SSR_MAX_STEPS 64
#define DEFAULT_TARGET_SHADER_ID 5
#define SSR_EDGE_FADE_START 0.05f
#define SSR_EDGE_FADE_END   0.95f
#define SSR_ROUGHNESS_SCALE 2.0f

// スクリーン端フェード — 端に近い反射サンプルを減衰させてちらつきを防ぐ
float ScreenEdgeFade(float2 uv)
{
    float2 fade = smoothstep(0.0f, SSR_EDGE_FADE_START, uv)
               * (1.0f - smoothstep(SSR_EDGE_FADE_END, 1.0f, uv));
    return fade.x * fade.y;
}

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

    // ShaderID でピクセルを識別
    // Parameter.w が未設定 (0) の場合はデフォルト ShaderID を使用
    int targetShaderID = (int) Parameter.w;
    if (targetShaderID <= 0)
        targetShaderID = DEFAULT_TARGET_SHADER_ID;

    uint paramW, paramH;
    GParam.GetDimensions(paramW, paramH);
    int2 pixelCoord = int2(uv.x * paramW, uv.y * paramH);
    uint4 param = GetObjParam(pixelCoord);
    uint shaderID = param.z;

    if ((int) shaderID != targetShaderID)
    {
        outDiffuse = sceneColor;
        return;
    }

    // ---- 対象ピクセル: SSR 実行 ----
    float3 worldPos = GetWorldPosition(uv);
    float3 worldNormal = normalize(GetWorldNormal(uv));
    float3 viewDir = normalize(CameraPosition.xyz - worldPos);
    float3 reflDir = normalize(reflect(-viewDir, worldNormal));

    float baseStepSize = max(Parameter.x, 0.1f);
    int maxSteps = clamp((int) Parameter.z, 8, SSR_MAX_STEPS);
    float intensity = saturate(Parameter.y);

    // カメラ距離に応じてステップサイズを調整
    float distToCamera = length(CameraPosition.xyz - worldPos);
    float adaptiveStep = baseStepSize * max(distToCamera * 0.05f, 0.5f);

    // Roughness を取得してぼかし量を決定
    float4 matParams = GetMaterial(uv);
    float roughness = saturate(matParams.g);

    // SSR レイマーチ（加速ステップ）
    float4 reflColor = float4(0, 0, 0, 0);
    bool hit = false;
    float marchDist = 0.0f;
    float hitFade = 1.0f;

    [loop]
    for (int i = 1; i <= maxSteps; i++)
    {
        // 加速ステップ: 距離が遠くなるにつれてステップを大きくする
        float step = adaptiveStep * (1.0f + float(i) * 0.15f);
        marchDist += step;

        float3 sampleWorldPos = worldPos + reflDir * marchDist;

        // ワールド → ビュー空間
        float4 viewPos4 = mul(float4(sampleWorldPos, 1.0f), View);

        // カメラの後ろに行ったら中断
        if (viewPos4.z <= 0.0f)
            break;

        // ビュー → クリップ空間
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

        // サンプル位置の深度を取得
        float sampleDepth = GetDepth(sampleUV);

        // スカイボックスは無視
        if (sampleDepth >= 1.0f)
            continue;

        float sampleLinearDepth = GetLinearDepth(sampleUV);
        float rayLinearDepth = viewPos4.z;
        float depthDiff = rayLinearDepth - sampleLinearDepth;

        // レイが表面を通過した場合（手前側に超えた）→ ヒット判定
        if (depthDiff > 0.0f && depthDiff < step * 3.0f)
        {
            reflColor = g_Texture.Sample(g_SamplerState, sampleUV);

            // スクリーン端フェードとレイ距離フェードを計算
            float edgeFade = ScreenEdgeFade(sampleUV);
            float distFade = 1.0f - saturate(float(i) / float(maxSteps));
            hitFade = edgeFade * distFade;

            hit = true;
            break;
        }
    }

    if (hit)
    {
        // Fresnel 計算
        float NdotV = saturate(dot(worldNormal, viewDir));
        float fresnel = 0.02f + 0.98f * pow(1.0f - NdotV, 5.0f);

        // Roughness で SSR 強度を減衰（粗い表面ほど SSR が弱い）
        float roughnessFade = 1.0f - saturate(roughness * SSR_ROUGHNESS_SCALE);

        // SSR ヒット: 反射色をシーンカラーにブレンド
        float blend = fresnel * intensity * hitFade * roughnessFade;
        outDiffuse = lerp(sceneColor, reflColor, blend);
    }
    else
    {
        // SSR ミス: マテリアルシェーダの環境マッピングがそのまま使用される
        outDiffuse = sceneColor;
    }
}
