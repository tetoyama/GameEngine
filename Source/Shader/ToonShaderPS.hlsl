#include "common.hlsl"

Texture2D g_Texture : register(t0);
SamplerState g_Sampler : register(s0);

// フォワード用関数（必要であれば共通化）
float CalculateGeometricDamping(float nh, float nv, float nl, float vh);
float CalculateDiffuseFromFresnel(float3 N, float3 L, float3 V);
float3 CalculateCookTorranceSpecular(float3 N, float3 L, float3 V, float smooth, float metallic);
float CalculateBeckmann(float m, float t);
float CalculateFresnel(float F0, float u);

void main(in PS_IN In, out float4 outColor : SV_Target)
{
    float3 normal = normalize(In.Normal.xyz);
    float3 lightDir = normalize(In.WorldPosition.xyz - Lights[0].Position.xyz);
    float3 viewDir = normalize(In.WorldPosition.xyz - CameraPosition.xyz);
    float3 halfDir = normalize(viewDir + lightDir);

    float3 texColor = g_Texture.Sample(g_Sampler, In.TexCoord).rgb;
    float3 albedo = Material.TextureEnable ? texColor * Material.Diffuse.rgb : Material.Diffuse.rgb;

        // ライト減衰
    float lightDist = length(Lights[0].Position.xyz - In.WorldPosition.xyz);
    float attenuation = saturate(1.0f - lightDist / Lights[0].PointLightParam.x);
    
    // ------- Toon 段階ライト -------
    float nl = saturate(dot(normal, lightDir));
    float lightFactor = -dot(normal, lightDir); // トゥーン風の逆転陰影
    lightFactor = (lightFactor + 1.0f) * attenuation;
    float toon = 1.0f;

    if (lightFactor < 0.15f)
    {
        toon = 0.3f;
    }
    else if (lightFactor < 0.35f)
    {
        float t = saturate((lightFactor - 0.15f) / 0.2f); // 0.15〜0.35 を 0〜1 にマッピング
        toon = lerp(0.3f, 0.6f, t);
    }
    else if (lightFactor < 1.0f)
    {
        toon = 0.6f;
    }
    else if (lightFactor < 1.2f)
    {
        float t = saturate((lightFactor - 1.0f) / 0.2f); // 1.0〜1.2 を 0〜1 にマッピング
        toon = lerp(0.6f, 0.8f, t);
    }
    else
    {
        float t = saturate((lightFactor - 1.2f) / 0.8f); // 1.0〜1.2 を 0〜1 にマッピング
        toon = lerp(0.8f, 1.0f, t);
    }



    // ------- PBRクックトレンス風スペキュラー -------
    float roughness = saturate(1.0f - Material.Shininess / 100.0f);
    float metallic = 0.5f;

    float3 specular = CalculateCookTorranceSpecular(normal, lightDir, viewDir, roughness, metallic) * Lights[0].Diffuse.rgb;
    specular *= attenuation;
    specular = min(specular, 0.25f); // トゥーンらしさを保つ制限

    // ------- リムライト -------
    float rim = 1.0f - max(0, dot(normal, -viewDir));
    rim = pow(rim, 2.0f) * Material.Emission.a;

    // ------- 環境光（スカイ/地面） -------
    float blend = normal.y * 0.5f + 0.5f;
    float4 hemiColor = lerp(Lights[0].GroundColor, Lights[0].SkyColor, blend);
    float3 env = hemiColor.rgb * hemiColor.a * 0.5f * Material.Specular.a;
    env += rim * Material.Diffuse.rgb * (texColor * 0.5f + 0.5f) * Material.Emission.rgb;

    // ------- 合成 -------
    float3 lighting = toon * In.Diffuse.rgb + Lights[0].Ambient.rgb;
    float3 result = albedo * lighting + specular + env;

    outColor.rgb = result;
    outColor.a = In.Diffuse.a; // テクスチャαと頂点α
}

// === PBR系関数（Cook-Torrance） ===
float CalculateGeometricDamping(float nh, float nv, float nl, float vh)
{
    return min(1.0f, min(2.0f * nh * nv / vh, 2.0f * nh * nl / vh));
}

float CalculateDiffuseFromFresnel(float3 N, float3 L, float3 V)
{
    float nl = saturate(dot(N, L));
    float nv = saturate(dot(N, V));
    return (nl * nv);
}

float3 CalculateCookTorranceSpecular(float3 N, float3 L, float3 V, float smooth, float metallic)
{
    float3 H = normalize(L + V);
    float nh = saturate(dot(N, H));
    float nl = saturate(dot(N, L));
    float nv = saturate(dot(N, V));
    float vh = saturate(dot(V, H));

    float D = CalculateBeckmann(metallic, nh);
    float G = CalculateGeometricDamping(nh, nv, nl, vh);
    float F = CalculateFresnel(smooth, vh);
    float denom = max(PI * nv * nh, 0.001f);

    return max(F * D * G / denom, 0.0f);
}

float CalculateBeckmann(float smooth, float nh)
{
    float nh2 = nh * nh;
    float denom = 4.0f * smooth * smooth * nh2 * nh2;
    return exp(-(1 - nh2) / (smooth * smooth * nh2)) / denom;
}

float CalculateFresnel(float F0, float u)
{
    return F0 + (1.0f - F0) * pow(1.0f - u, 5.0f);
}
