#include "common.hlsl"

Texture2D g_Texture : register(t0);
Texture2D g_NormalMap : register(t1);
SamplerState g_Sampler : register(s0);

// PI 定義
#define PI 3.14159265f

// Fresnel
float3 FresnelSchlick(float cosTheta, float3 F0)
{
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

float DistributionGGX(float3 N, float3 H, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    return a2 / (PI * denom * denom);
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    return NdotV / (NdotV * (1.0 - k) + k);
}

float GeometrySmith(float3 N, float3 V, float3 L, float roughness)
{
    float ggx1 = GeometrySchlickGGX(max(dot(N, V), 0.0), roughness);
    float ggx2 = GeometrySchlickGGX(max(dot(N, L), 0.0), roughness);
    return ggx1 * ggx2;
}

void main(in PS_IN In, out float4 outColor : SV_Target)
{
    // Albedo (Diffuse Color)
    float3 albedo = Material.Diffuse.rgb;
    if (Material.TextureEnable)
    {
        float4 texColor = g_Texture.Sample(g_Sampler, In.TexCoord);
        albedo *= texColor.rgb;
    }

    // Normal Mapping
    float3 normalMap = g_NormalMap.Sample(g_Sampler, In.TexCoord).rgb;
    normalMap = normalMap * 2.0f - 1.0f; // [-1,1]
    float3 N = normalize(normalMap + In.Normal.xyz); // スケーリング無しの簡易合成

    // View, Light, Half vectors
    float3 V = normalize(CameraPosition.xyz - In.WorldPosition.xyz);
    float3 L = normalize(Light.Position.xyz - In.WorldPosition.xyz);
    float3 H = normalize(V + L);

    float NdotL = max(dot(N, L), 0.0);
    float NdotV = max(dot(N, V), 0.0);
    float NdotH = max(dot(N, H), 0.0);
    float VdotH = max(dot(V, H), 0.0);

    // 光減衰
    float dist = length(Light.Position.xyz - In.WorldPosition.xyz);
    float attenuation = saturate(1.0f - dist / Light.PointLightParam.x);

    // Specular base reflectance (F0)
    float3 F0 = Material.Specular.rgb;
    if (F0.r == 0 && F0.g == 0 && F0.b == 0)
        F0 = float3(0.04, 0.04, 0.04); // ダイレクトに鏡面反射が設定されていない場合のデフォルト

    float roughness = saturate(1.0f - Material.Shininess / 128.0f);

    // BRDF計算
    float D = DistributionGGX(N, H, roughness);
    float G = GeometrySmith(N, V, L, roughness);
    float3 F = FresnelSchlick(VdotH, F0);

    float3 specular = D * G * F / (4.0 * NdotL * NdotV + 0.001);
    float3 kD = 1.0 - F;
    kD *= 1.0 - 0.0; // 非金属として扱う（metallic = 0）

    float3 ambient = Light.Ambient.rgb * Material.Ambient.rgb;
    float3 diffuse = kD * albedo / PI;

    float3 color = (diffuse + specular) * Light.Diffuse.rgb * NdotL * attenuation;
    color += ambient;
    color += Material.Emission.rgb;

    // ガンマ補正
    color = pow(color, 1.0 / 2.2);
    outColor = float4(color, 1.0);
}
