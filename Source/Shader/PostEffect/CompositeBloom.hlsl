#include "../common.hlsl"

Texture2D g_OriginalTexture : register(t0);
Texture2D g_BloomTexture : register(t1);
SamplerState g_SamplerState : register(s0);

// ACES Filmic Tone Mapping（Krzysztof Narkowicz 近似式）
// HDR 値を知覚的に自然な LDR カーブに変換する
// 参考: https://knarkowicz.wordpress.com/2016/01/06/aces-filmic-tone-mapping-curve/
float3 ACESFilmic(float3 x)
{
    // y = (x * (a*x + b)) / (x * (c*x + d) + e)
    // a=2.51, b=0.03, c=2.43, d=0.59, e=0.14 (Narkowicz ACES 近似定数)
    const float a = 2.51f;
    const float b = 0.03f;
    const float c = 2.43f;
    const float d = 0.59f;
    const float e = 0.14f;
    return saturate((x * (a * x + b)) / (x * (c * x + d) + e));
}

float4 main(PS_IN In) : SV_Target
{
    // Parameter.x : ブルーム強度オフセット  (0 → 1倍, -1 → ブルームなし)
    // Parameter.y : 露出オフセット           (0 → 1倍, 1 → 2倍明るく)
    float BloomIntensity = Parameter.x + 1.0f;
    float Exposure       = Parameter.y + 1.0f;

    float4 original = g_OriginalTexture.Sample(g_SamplerState, In.TexCoord);
    float4 bloom    = g_BloomTexture.Sample(g_SamplerState, In.TexCoord);

    // HDR 合成: シーンカラー + ブルーム
    float3 hdr = (original.rgb + bloom.rgb * BloomIntensity) * Exposure;

    // ACES Filmic トーンマッピング（HDR → LDR）
    float3 toneMapped = ACESFilmic(hdr);

    return float4(toneMapped, original.a);
}

