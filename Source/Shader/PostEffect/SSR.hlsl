#include "../common.hlsl"
#include "PostEffectFunc.hlsli" 

// ============================================================
// SSR (Screen Space Reflections) Pixel Shader
//
// GBuffer のワールド座標・法線からビュー空間で反射レイをマーチし、
// スクリーン上で交差したピクセルの色を反射色として合成する。
// フレネル係数とスクリーン端フェードにより自然な反射を実現する。
//
// Parameter.x : レイマーチ最大ステップ数 (推奨値: 16)
// Parameter.y : 1ステップの移動量 (ビュー空間, 推奨値: 2)
// Parameter.z : 交差判定の厚みしきい値 (推奨値: 8)
// ============================================================
// ライティング済みシーンテクスチャ (t0)
Texture2D g_Texture : register(t0);
// 疑似乱数生成関数 (IGN: Interleaved Gradient Noise)
// ラフネスによるレイの分散に使用
float InterleavedGradientNoise(float2 uv)
{
    float3 magic = float3(0.06711056f, 0.00583715f, 52.9829189f);
    return frac(magic.z * frac(dot(uv, magic.xy)));
}

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

    // GBuffer から情報を取得
    float3 worldPos = GetWorldPosition(uv);
    float3 worldNormal = normalize(GetWorldNormal(uv));
    float3 viewDir = normalize(CameraPosition.xyz - worldPos);

    // マテリアル情報の取得
    float4 material = GetMaterial(uv);
    float metallic = material.x;
    float roughness = material.y;
    float reflStrength = saturate(metallic); // メタリック度を反射強度として使用

    // フレネル係数: ラフネスを考慮した Schlick-GGX 近似に近い形に調整
    float fresnel = pow(1.0f - saturate(dot(worldNormal, viewDir)), 4.0f) * 0.98f + 0.02f;
    // ラフネスが強いほどフレネル反射も微弱に拡散するため、少し減衰させる処理
    fresnel *= (1.0f - roughness * 0.5f);

    // ビュー空間へ変換
    float3 viewPos = mul(float4(worldPos, 1.0f), View).xyz;
    float3 viewNormal = normalize(mul(float4(worldNormal, 0.0f), View).xyz);
    float3 viewDirVS = normalize(mul(float4(viewDir, 0.0f), View).xyz);

    // ---- ラフネスによる反射ベクトルのジッタリング ----
    float3 reflDir = reflect(-viewDirVS, viewNormal);
    
    // 疑似乱数を用いて反射方向をラフネスに応じて散らす
    float noise = InterleavedGradientNoise(In.Position.xy);
    float3 jitter = (noise * 2.0f - 1.0f) * roughness * 0.1f; // 0.1は拡散の影響度調整
    reflDir = normalize(reflDir + jitter);

    // パラメータ取得
    int maxSteps = (int) clamp(Parameter.x, 4.0f, 64.0f);
    float stepSize = max(Parameter.y, 0.001f);
    float thickness = max(Parameter.z, 0.01f);

    // ---- レイマーチ ----
    float3 currentPos = viewPos;
    float2 hitUV = (float2) 0;
    bool hit = false;

    [loop]
    for (int i = 0; i < maxSteps; i++)
    {
        float t = (float) (i + 1) / (float) maxSteps;
        currentPos += reflDir * stepSize * (1.0f + t * 2.0f);

        float4 clipPos = mul(float4(currentPos, 1.0f), Projection);
        if (clipPos.w <= 0.0f)
            break;
        clipPos.xyz /= clipPos.w;

        float2 sampleUV = float2(
             clipPos.x * 0.5f + 0.5f,
            -clipPos.y * 0.5f + 0.5f
        );

        if (sampleUV.x < 0.0f || sampleUV.x > 1.0f ||
            sampleUV.y < 0.0f || sampleUV.y > 1.0f)
            break;

        float3 sampleWorldPos = GetWorldPosition(sampleUV);
        float sampleViewZ = mul(float4(sampleWorldPos, 1.0f), View).z;

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
        // ---- ラフネスによるぼかし (Mipmap Sampling) ----
        // ラフネスが高いほど、解像度の低い（ぼけた）ミップレベルをサンプリングする
        // ※g_Texture にミップマップが生成されていることが前提
        float maxMipLevel = 5.0f; // ぼかしの最大深度
        float mipLevel = roughness * maxMipLevel;
        float4 reflColor = g_Texture.SampleLevel(g_SamplerState, hitUV, mipLevel);

        // スクリーン端フェードアウト
        float2 edgeFade2 = min(hitUV, 1.0f - hitUV) * 5.0f;
        float edgeFade = saturate(min(edgeFade2.x, edgeFade2.y));

        // 反射ベクトルの向きがカメラ方向を向いている場合の減衰 (セルフリフレクション防止)
        float viewFacingFade = saturate(1.0f - max(0.0f, dot(reflDir, viewDirVS)));

        float blendFactor = fresnel * reflStrength * edgeFade * viewFacingFade;
        
        // ラフネスが非常に高い場合は反射をさらに弱める
        blendFactor *= (1.0f - roughness * roughness);

        outDiffuse = lerp(sceneColor, reflColor, blendFactor);
    }
    else
    {
        outDiffuse = sceneColor;
    }
}