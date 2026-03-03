#include "../common.hlsl"
#include "PostEffectFunc.hlsli"

// ============================================================
// Water SSR (Screen Space Reflections) Post-Effect
//
// WaveComponent の水面ピクセルに対して SSR を適用し、
// SSR が外れた場合もシーンカラーをそのまま出力する
// （環境反射は WaterSurfaceShader で既に適用済み）
//
// Parameter.x : SSR ステップサイズ       (推奨値: 1.0)
// Parameter.y : 反射強度                 (推奨値: 0.8)
// Parameter.z : SSR 最大イテレーション数 (推奨値: 48)
// Parameter.w : 水面の ShaderID          (水面マテリアルの ID と一致させる)
// ============================================================

Texture2D g_Texture : register(t0);

#define SSR_MAX_STEPS 64
#define WATER_SURFACE_SHADER_ID 5

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
    // Parameter.w が未設定 (0) の場合はデフォルトで WaterSurface の ShaderID を使用
    int waterShaderID = (int) Parameter.w;
    if (waterShaderID <= 0)
        waterShaderID = WATER_SURFACE_SHADER_ID;

    uint paramW, paramH;
    GParam.GetDimensions(paramW, paramH);
    int2 pixelCoord = int2(uv.x * paramW, uv.y * paramH);
    uint4 param = GetObjParam(pixelCoord);
    uint shaderID = param.z;

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

    float baseStepSize = max(Parameter.x, 0.1f);
    int maxSteps = clamp((int) Parameter.z, 8, SSR_MAX_STEPS);
    float intensity = saturate(Parameter.y);

    // 水面からカメラまでの距離に応じてステップサイズを調整
    float distToCamera = length(CameraPosition.xyz - worldPos);
    float adaptiveStep = baseStepSize * max(distToCamera * 0.05f, 0.5f);

    // SSR レイマーチ（加速ステップ）
    float4 reflColor = float4(0, 0, 0, 0);
    bool hit = false;
    float marchDist = 0.0f;

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
            // ヒット: 反射されたピクセルの色を取得
            reflColor = g_Texture.Sample(g_SamplerState, sampleUV);
            hit = true;
            break;
        }
    }

    if (hit)
    {
        // Fresnel 計算
        float NdotV = saturate(dot(worldNormal, viewDir));
        float fresnel = 0.02f + 0.98f * pow(1.0f - NdotV, 5.0f);

        // SSR ヒット: 反射色をシーンカラーにブレンド
        // SSR の反射はマテリアルシェーダの環境マッピングを上書き
        outDiffuse = lerp(sceneColor, reflColor, fresnel * intensity);
    }
    else
    {
        // SSR ミス: マテリアルシェーダの環境マッピングがそのまま使用される
        outDiffuse = sceneColor;
    }
}
