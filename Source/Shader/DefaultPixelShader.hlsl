#include "common.hlsl"

Texture2D g_Texture : register(t0);
Texture2D g_NormalMap : register(t1);
Texture2D ShadowMap : register(t2);

SamplerState g_Sampler : register(s0);
SamplerComparisonState ShadowSampler : register(s1);

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

// ---- Shadow sampling ----
float ShadowFactor(float4 worldPos, LIGHT light)
{
    float4 shadowPos = mul(float4(worldPos.xyz, 1.0f), light.LightView);
    shadowPos = mul(shadowPos, light.LightProjection);

    shadowPos.xyz /= shadowPos.w;

    float2 uv = shadowPos.xy * 0.5f + 0.5f;
    float depth = shadowPos.z;

    if (uv.x < 0.0f || uv.x > 1.0f || uv.y < 0.0f || uv.y > 1.0f)
        return 1.0f;

    float shadow = ShadowMap.SampleCmpLevelZero(ShadowSampler, uv, depth);

    return shadow;
}

float4 main(in PS_IN In) : SV_Target
{
    float3 baseColor = Material.Diffuse.rgb;
    if (Material.TextureEnable)
    {
        float4 texColor = g_Texture.Sample(g_Sampler, In.TexCoord);
        baseColor *= texColor.rgb;
    }

    float3 N = normalize(In.Normal.xyz);
    float3 V = normalize(CameraPosition.xyz - In.WorldPosition.xyz);
    float3 L = normalize(Lights[0].Position.xyz - In.WorldPosition.xyz);
    float3 H = normalize(V + L);

    float NdotL = max(dot(N, L), 0.0);
    float NdotV = max(dot(N, V), 0.0);
    float NdotH = max(dot(N, H), 0.0);
    float LdotH = max(dot(L, H), 0.0);

    float3 F0 = lerp(float3(0.04, 0.04, 0.04), baseColor, 0.0);

    float perceptualRoughness = saturate(1.0 - Material.Shininess / 128.0);
    float roughness = max(perceptualRoughness, 0.05);

    float3 F = FresnelSchlick(LdotH, F0);
    float G = G_Smith(N, V, L, roughness);
    float D = D_GTR2(NdotH, roughness);

    float3 specular = (D * F * G) / (4.0 * NdotL * NdotV + 0.001);
    float specScale = 0.5 + 0.5 * roughness;
    specular *= specScale;

    float FL = pow(1.0 - NdotL, 5.0);
    float FV = pow(1.0 - NdotV, 5.0);
    float3 diffuseTerm = baseColor * (1.0 + FL) * (1.0 + FV) * (1.0 / PI);

    float dist = length(Lights[0].Position.xyz - In.WorldPosition.xyz);
    float attenuation = saturate(1.0f - dist / Lights[0].Param.x);

    // ---- shadow計算 ----
    float shadow = ShadowFactor(In.WorldPosition, Lights[0]);

    float3 Lo = (diffuseTerm + specular) * Lights[0].Diffuse.rgb * NdotL * attenuation * shadow;

    float3 ambient = Material.Ambient.rgb * Lights[0].Ambient.rgb;
    float3 emission = Material.Emission.rgb;

    float3 finalColor = ambient + Lo + emission;

    finalColor = pow(finalColor, 1.0 / 2.2);
    return float4(finalColor, 1.0);
    //outColor = float4(shadow, shadow, shadow, 1.0);
    
}
