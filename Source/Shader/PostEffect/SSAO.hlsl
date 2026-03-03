#include "../common.hlsl"
#include "PostEffectFunc.hlsli"

// ============================================================
// SSAO (Screen Space Ambient Occlusion) Pixel Shader
//
// GBuffer から深度・法線・ワールド座標を取得し、
// ビュー空間の半球サンプリングにより遮蔽度を算出する。
// 結果をシーンカラーに乗算して出力する。
//
// Parameter.x : サンプリング半径    (推奨値: 0.5)
// Parameter.y : 深度バイアス        (推奨値: 0.025)
// Parameter.z : 遮蔽強度 (pow乗数) (推奨値: 2.0)
// ============================================================

// ライティング済みシーンテクスチャ (t0)
Texture2D g_Texture : register(t0);

// サンプルカーネル数
#define SSAO_KERNEL_SIZE 32

// 接線空間における半球サンプルを生成する
// i      : サンプルインデックス [0, SSAO_KERNEL_SIZE)
// scale  : 距離スケール (近くに多く集中させるため非線形)
// 戻り値 : 接線空間での正規化サンプルベクトル * scale
float3 GenKernelSample(int i, float scale)
{
    // 決定論的疑似乱数でサンプル方向を生成
    float xi1 = frac(sin(float(i) * 127.1f + 31.7f)  * 43758.5453f) * 2.0f - 1.0f;
    float xi2 = frac(sin(float(i) * 269.5f + 183.3f) * 43758.5453f) * 2.0f - 1.0f;
    float xi3 = frac(sin(float(i) * 419.2f + 97.5f)  * 43758.5453f);          // [0,1] 上半球
    return normalize(float3(xi1, xi2, xi3)) * scale;
}

void main(in PS_IN In, out float4 outDiffuse : SV_Target)
{
    float2 uv = In.TexCoord;

    // シーンカラー取得
    float4 sceneColor = g_Texture.Sample(g_SamplerState, uv);

    // スカイボックス・未書き込みピクセルをスキップ
    float depth = GetDepth(uv);
    if (depth >= 1.0f)
    {
        outDiffuse = sceneColor;
        return;
    }

    // GBuffer からワールド空間の座標・法線を取得
    float3 worldPos    = GetWorldPosition(uv);
    float3 worldNormal = GetWorldNormal(uv);   // [-1,1] デコード済み

    // SSAO パラメータ
    float radius   = max(Parameter.x, 0.001f);
    float bias     = max(Parameter.y, 0.0f);
    float strength = max(Parameter.z, 0.0f);

    // ビュー空間へ変換 (行ベクトル × 行列 規約)
    float3 viewPos    = mul(float4(worldPos,    1.0f), View).xyz;
    float3 viewNormal = normalize(mul(float4(worldNormal, 0.0f), View).xyz);

    // UV ベースのランダム回転ベクトル (ピクセルごとにカーネルを回転)
    float noiseX   = frac(sin(dot(uv, float2(127.1f, 311.7f))) * 43758.5453f);
    float noiseY   = frac(sin(dot(uv, float2(269.5f, 183.3f))) * 43758.5453f);
    float3 randVec = normalize(float3(noiseX * 2.0f - 1.0f, noiseY * 2.0f - 1.0f, 0.0f));

    // TBN 行列: 接線空間 → ビュー空間
    float3   tangent   = normalize(randVec - viewNormal * dot(randVec, viewNormal));
    float3   bitangent = cross(viewNormal, tangent);
    float3x3 TBN       = float3x3(tangent, bitangent, viewNormal);

    // AO 値の積算
    float occlusion = 0.0f;

    [loop]
    for (int i = 0; i < SSAO_KERNEL_SIZE; i++)
    {
        // 加速補間: 近くのサンプルを多くする
        float t     = (float(i) + 1.0f) / float(SSAO_KERNEL_SIZE);
        float scale = lerp(0.1f, 1.0f, t * t);

        // 接線空間サンプル → ビュー空間サンプル位置
        float3 kernelVS  = mul(GenKernelSample(i, scale), TBN);
        float3 sampleVS  = viewPos + kernelVS * radius;

        // ビュー空間 → クリップ空間 → NDC → UV
        float4 sampleClip = mul(float4(sampleVS, 1.0f), Projection);
        sampleClip.xyz   /= sampleClip.w;
        float2 sampleUV   = float2(
             sampleClip.x * 0.5f + 0.5f,
            -sampleClip.y * 0.5f + 0.5f
        );

        // スクリーン外のサンプルは無視
        if (sampleUV.x < 0.0f || sampleUV.x > 1.0f ||
            sampleUV.y < 0.0f || sampleUV.y > 1.0f)
            continue;

        // GBuffer からサンプル位置のビュー深度を取得
        float3 sampleWorldPos = GetWorldPosition(sampleUV);
        float  sampleViewZ    = mul(float4(sampleWorldPos, 1.0f), View).z;

        // 範囲チェック: 深度差がradius以上離れていれば遮蔽に含めない
        float rangeCheck = smoothstep(0.0f, 1.0f,
            radius / max(abs(viewPos.z - sampleViewZ), 1e-5f));

        // 遮蔽判定: GBufferの表面がサンプル点より手前にある場合に遮蔽と判断
        // DirectX LH 規約: z は正方向がカメラから奥へ向かう
        occlusion += (sampleViewZ < sampleVS.z - bias ? 1.0f : 0.0f) * rangeCheck;
    }

    // 正規化・強度適用
    occlusion = 1.0f - (occlusion / float(SSAO_KERNEL_SIZE));
    occlusion = pow(saturate(occlusion), strength);

    outDiffuse = float4(sceneColor.rgb * occlusion, sceneColor.a);
}
