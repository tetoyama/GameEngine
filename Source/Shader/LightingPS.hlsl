#include <commonDefine.h>
#include "common.hlsl"

// Lighting Slot 定義（PS側）
Texture2D GAlbedo : register(t0);
Texture2D GNormal : register(t1);
Texture2D GPosition : register(t2);
Texture2D GMaterial : register(t3);
Texture2D<uint4> GParam : register(t4);

Texture2D ShadowMap : register(t5);

SamplerState LinearSampler : register(s0);
SamplerComparisonState ShadowSampler : register(s1);

float4 main(PS_IN In) : SV_Target0
{
    return float4(In.TexCoord, 0, 1);

    // -----------------------------
    // GBuffer 読み取り
    // -----------------------------
    float4 albedo = GAlbedo.Sample(LinearSampler, In.TexCoord);
    float3 normal = GNormal.Sample(LinearSampler, In.TexCoord).xyz;
    float3 worldPos = GPosition.Sample(LinearSampler, In.TexCoord).xyz;
    float4 material = GMaterial.Sample(LinearSampler, In.TexCoord);

    // Decode normal [0,1] -> [-1,1]
    normal = normalize(normal * 2.0f - 1.0f);

    // -----------------------------
    // 仮ライト（1灯・固定）
    // -----------------------------
    float3 lightDir = normalize(float3(0.5f, -1.0f, 0.3f));
    float3 lightCol = float3(1.0f, 1.0f, 1.0f);

    float NdotL = saturate(dot(normal, -lightDir));

    // -----------------------------
    // Lambert
    // -----------------------------
    float3 diffuse = albedo.rgb * lightCol * NdotL;

    // 環境光（最低限）
    float3 ambient = albedo.rgb * 0.1f;

    float3 color = diffuse + ambient;

    return float4(color, 1.0f);
}
