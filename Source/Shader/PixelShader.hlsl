#include "common.hlsl"

Texture2D g_Texture : register(t0);
Texture2D g_NormalMap : register(t1);
SamplerState g_Sampler : register(s0);

#define PI 3.14159265f

float3 FresnelSchlick(float cosTheta, float3 F0)
{
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

float SchlickGGX(float NdotV, float roughness)
{
    float a = roughness;
    float k = (a * a) / 2.0;
    return NdotV / (NdotV * (1.0 - k) + k);
}

float G_Smith(float3 N, float3 V, float3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx1 = SchlickGGX(NdotV, roughness);
    float ggx2 = SchlickGGX(NdotL, roughness);
    return ggx1 * ggx2;
}

float D_GTR2(float NdotH, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float denom = NdotH * NdotH * (a2 - 1.0) + 1.0;
    return a2 / (PI * denom * denom);
}

void main(in PS_IN In, out float4 outColor : SV_Target)
{
    float3 baseColor = Material.Diffuse.rgb;
    if (Material.TextureEnable)
    {
        float4 texColor = g_Texture.Sample(g_Sampler, In.TexCoord);
        baseColor *= texColor.rgb;
    }

    float3 N = normalize(In.Normal.xyz);
    float3 V = normalize(CameraPosition.xyz - In.WorldPosition.xyz);
    float3 L = normalize(Light.Position.xyz - In.WorldPosition.xyz);
    float3 H = normalize(V + L);

    float NdotL = max(dot(N, L), 0.0);
    float NdotV = max(dot(N, V), 0.0);
    float NdotH = max(dot(N, H), 0.0);
    float LdotH = max(dot(L, H), 0.0);

    // Disneyの Specular F0
    float3 F0 = lerp(float3(0.04, 0.04, 0.04), baseColor, 0.0); // metallic = 0 と仮定

    // roughness (Shininessから変換)
    float perceptualRoughness = saturate(1.0 - Material.Shininess / 128.0);
    float roughness = max(perceptualRoughness, 0.05); // 0は使えない

    // Fresnel, Geometry, Distribution
    float3 F = FresnelSchlick(LdotH, F0);
    float G = G_Smith(N, V, L, roughness);
    float D = D_GTR2(NdotH, roughness);

    float3 specular = (D * F * G) / (4.0 * NdotL * NdotV + 0.001);
    float specScale = 0.5 + 0.5 * roughness; // roughなほど光沢控えめ
    specular *= specScale;
    
    // Diffuse: DisneyDiffuse
    float FL = pow(1.0 - NdotL, 5.0);
    float FV = pow(1.0 - NdotV, 5.0);
    float3 diffuseTerm = baseColor * (1.0 + FL) * (1.0 + FV) * (1.0 / PI);

    // 光の減衰（距離による）
    float dist = length(Light.Position.xyz - In.WorldPosition.xyz);
    float attenuation = saturate(1.0f - dist / Light.PointLightParam.x);

    // Light color * BRDF
    float3 Lo = (diffuseTerm + specular) * Light.Diffuse.rgb * NdotL * attenuation;

    // Ambient and Emission
    float3 ambient = Material.Ambient.rgb * Light.Ambient.rgb;
    float3 emission = Material.Emission.rgb;

    float3 finalColor = ambient + Lo + emission;

    finalColor = pow(finalColor, 1.0 / 2.2); // gamma correction
    outColor = float4(finalColor, 1.0);
}
