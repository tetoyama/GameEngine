#include "common.hlsl"

Texture2D g_Texture : register(t0);
//Texture2D g_NormalMap : register(t1);
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

float ShadowFactor(float3 worldPos, LIGHT light)
{
    float4 shadowPos = mul(float4(worldPos, 1.0f), light.LightView);
    shadowPos = mul(shadowPos, light.LightProjection);

    shadowPos.xyz /= shadowPos.w;
    float2 uv = shadowPos.xy * 0.5f + 0.5f;
    uv.y = 1.0f - uv.y;

    float depthBias = 0.001;
    float depth = saturate(shadowPos.z - depthBias);

    if (uv.x < 0.0f || uv.x > 1.0f || uv.y < 0.0f || uv.y > 1.0f)
        return 1.0f;

    uint width, height;
    ShadowMap.GetDimensions(width, height);
    float2 texelSize = 1.0f / float2(width, height);

    int sample = 2;
    float shadow = 0.0f;
    float totalWeight = 0.0f;
    float kernel[5] = { 0.06136, 0.24477, 0.38774, 0.24477, 0.06136 };

    [unroll]
    for (int x = -sample; x <= sample; x++)
    {
        [unroll]
        for (int y = -sample; y <= sample; y++)
        {
            float2 offset = float2(x, y) * texelSize;
            float weight = kernel[x + sample] * kernel[y + sample];
            shadow += ShadowMap.SampleCmpLevelZero(ShadowSampler, uv + offset, depth) * weight;
            totalWeight += weight;
        }
    }

    shadow /= totalWeight;
    return shadow;
}

float3 ComputeLightContribution(LIGHT light, float3 N, float3 V, float3 worldPos, float3 baseColor)
{
    float3 L;
    float attenuation = 1.0;

    if (light.LightType == LIGHT_TYPE_DIRECTIONAL)
    {
        L = normalize(-light.Direction.xyz);
    }
    else
    {
        L = normalize(light.Position.xyz - worldPos);
        float dist = length(light.Position.xyz - worldPos);
        attenuation = saturate(1.0f - dist / light.Param.x);

        if (light.LightType == LIGHT_TYPE_SPOT)
        {
            float3 spotDir = normalize(-light.Direction.xyz);
            float cosTheta = dot(L, spotDir);
            float inner = light.Param.y;
            float outer = light.Param.z;
            float spotFactor = saturate((cosTheta - outer) / (inner - outer));
            attenuation *= spotFactor;
        }
    }

    float3 H = normalize(V + L);
    float NdotL = max(dot(N, L), 0.0);
    float NdotV = max(dot(N, V), 0.0);
    float NdotH = max(dot(N, H), 0.0);
    float LdotH = max(dot(L, H), 0.0);

    float3 F0 = lerp(float3(0.04, 0.04, 0.04), baseColor, 0.0);
    float roughness = max(saturate(1.0 - Material.Shininess / 128.0), 0.05);

    float3 F = FresnelSchlick(LdotH, F0);
    float G = G_Smith(N, V, L, roughness);
    float D = D_GTR2(NdotH, roughness);

    float3 specular = (D * F * G) / (4.0 * NdotL * NdotV + 0.001);
    specular = saturate(specular);

    float FL = pow(1.0 - NdotL, 5.0);
    float FV = pow(1.0 - NdotV, 5.0);
    float3 diffuseTerm = (1.0 + FL) * (1.0 + FV) * (1.0 / PI);

    float shadow = ShadowFactor(worldPos, light);

    float3 diffuse = baseColor * diffuseTerm * light.Diffuse.rgb * NdotL * attenuation * shadow;
    float3 spec = specular * light.Diffuse.rgb * shadow * attenuation;

    return diffuse + spec;
}

float4 main(in PS_IN In) : SV_Target
{
    float3 baseColor = Material.Diffuse.rgb;
    if (Material.TextureEnable)
    {
        baseColor *= g_Texture.Sample(g_Sampler, In.TexCoord).rgb;
    }

    float3 N = normalize(In.Normal.xyz);
    float3 V = normalize(CameraPosition.xyz - In.WorldPosition.xyz);

    float3 Lo = float3(0, 0, 0);
    float3 ambient = float3(0, 0, 0);

    for (int i = 0; i < LIGHT_MAX_COUNT; i++)
    {
        LIGHT light = Lights[i];
        if (light.Enable == 0)
            continue;

        ambient += baseColor * light.Ambient.rgb;
        Lo += ComputeLightContribution(light, N, V, In.WorldPosition.xyz, baseColor);
    }

    float3 finalColor = ambient + Lo + Material.Emission.rgb;
    finalColor = pow(finalColor, 1.0 / 2.2);

    return float4(finalColor, 1.0);
}
