#include "common.hlsl"
#include "commondefine.h"

// ============================================================================
// GBuffer
// ============================================================================
Texture2D GAlbedo : register(t0);
Texture2D GNormal : register(t1);
Texture2D GPosition : register(t2);
Texture2D GMaterial : register(t3);
Texture2D<uint4> GParam : register(t4);

Texture2D ShadowMap : register(t5);

SamplerState g_Sampler : register(s0);
SamplerComparisonState ShadowSampler : register(s1);
SamplerState LinearSampler : register(s2);

// ============================================================================
// PBR Utility
// ============================================================================
float3 FresnelSchlick(float cosTheta, float3 F0)
{
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

float SchlickGGX(float NdotV, float roughness)
{
    float a = roughness;
    float k = (a * a) * 0.5;
    return NdotV / (NdotV * (1.0 - k) + k);
}

float G_Smith(float3 N, float3 V, float3 L, float roughness)
{
    float NdotV = saturate(dot(N, V));
    float NdotL = saturate(dot(N, L));
    return SchlickGGX(NdotV, roughness) * SchlickGGX(NdotL, roughness);
}

float D_GTR2(float NdotH, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float d = (NdotH * NdotH) * (a2 - 1.0) + 1.0;
    return a2 / (PI * d * d);
}

// ============================================================================
// Shadow
// ============================================================================
float ShadowFactor(float3 worldPos, LIGHT light, int lightIndex)
{
    float4 sp = mul(float4(worldPos, 1), light.LightView);
    sp = mul(sp, light.LightProjection);
    sp.xyz /= sp.w;

    float2 uv = sp.xy * 0.5 + 0.5;
    uv.y = 1.0 - uv.y;

    float depth = saturate(sp.z - 0.001);

    uint w, h;
    ShadowMap.GetDimensions(w, h);

    uint grid = ceil(sqrt((float) LIGHT_MAX_COUNT));
    float tile = 1.0 / grid;

    uint gx = lightIndex % grid;
    uint gy = lightIndex / grid;

    float2 tileMin = float2(gx, gy) * tile;
    float2 tileMax = tileMin + tile;

    float2 suv = tileMin + uv * tile;

    if (any(suv < tileMin) || any(suv > tileMax))
        return 1.0;

    return ShadowMap.SampleCmpLevelZero(ShadowSampler, suv, depth);
}

// ============================================================================
// Lighting
// ============================================================================
float3 ComputeLightContribution(
    LIGHT light,
    float3 N,
    float3 V,
    float3 worldPos,
    float3 baseColor,
    float shininess,
    int lightIndex)
{
    float3 L;
    float attenuation = 1.0;

    if (light.LightType == LIGHT_TYPE_DIRECTIONAL)
    {
        L = normalize(-light.Direction.xyz);
    }
    else
    {
        float3 toL = light.Position.xyz - worldPos;
        float dist = length(toL);
        L = toL / max(dist, 0.001);

        attenuation = saturate(1.0 - dist / max(light.Param.x, 0.001));
    }

    float3 H = normalize(V + L);

    float NdotL = saturate(dot(N, L));
    float NdotV = saturate(dot(N, V));
    float NdotH = saturate(dot(N, H));
    float LdotH = saturate(dot(L, H));

    float roughness = max(1.0 - shininess / 128.0, 0.05);

    float3 F0 = float3(0.04, 0.04, 0.04);
    float3 F = FresnelSchlick(LdotH, F0);
    float G = G_Smith(N, V, L, roughness);
    float D = D_GTR2(NdotH, roughness);

    float3 spec =
        (D * F * G) / max(4.0 * NdotL * NdotV, 0.001);

    float3 diff = baseColor * (1.0 / PI);

    float shadow = ShadowFactor(worldPos, light, lightIndex);

    return (diff + spec) * light.Diffuse.rgb * NdotL * attenuation * shadow;
}

// ============================================================================
// Pixel Shader Main
// ============================================================================
float4 main(PS_IN In) : SV_Target
{
    float2 uv = In.TexCoord;
    //return float4(uv, 1.0, 1.0);

    float3 baseColor = GAlbedo.Sample(g_Sampler, uv).rgb;
    float3 N = GNormal.Sample(g_Sampler, uv).rgb;
    float3 worldPos = GPosition.Sample(g_Sampler, uv).rgb;
    float4 mat = GMaterial.Sample(g_Sampler, uv);

    int2 pixelCoord = int2(In.Position.xy);
    uint4 param = GParam.Load(int3(pixelCoord, 0));
    
    int sceneID = (int) param.x;
    int objectID = (int) param.y;
    int materialID = (int) param.z;
    
    if (materialID == 0)
    {
        return float4(1.0, 0.0, 1.0, 1.0);
    }
    
    N = normalize(N * 2.0 - 1.0);

    float shininess = mat.r * 128.0;
    float emission = mat.g;

    float3 V = normalize(CameraPosition.xyz - worldPos);

    float3 color = float3(0, 0, 0);
    float3 ambient = float3(0, 0, 0);

    for (int i = 0; i < LIGHT_MAX_COUNT; i++)
    {
        if (Lights[i].Enable == 0)
            continue;

        ambient += baseColor * Lights[i].Ambient.rgb;
        color += ComputeLightContribution(
            Lights[i],
            N,
            V,
            worldPos,
            baseColor,
            shininess,
            i);
    }

    float3 finalColor = ambient + color + emission;
    finalColor = pow(finalColor, 1.0 / 2.2);

    return float4(finalColor, 1.0);
}
