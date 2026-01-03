#include "../common.hlsl"
#include "../commondefine.h"
#include "../Material/MaterialFunc.hlsli"

// ============================================================================
// GBuffer
// ============================================================================
Texture2D           GAlbedo     : register(t0);
Texture2D           GNormal     : register(t1);
Texture2D           GPosition   : register(t2);
Texture2D           GMaterial   : register(t3);
Texture2D<uint4>    GParam      : register(t4);

Texture2D           ShadowMap   : register(t5);

SamplerState            LinearSampler   : register(s0);
SamplerComparisonState  ShadowSampler   : register(s1);

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


    uint grid = (uint) ceil(sqrt((float) LIGHT_MAX_COUNT));
    uint uIndex = (uint) lightIndex;
    uint gx = uIndex % grid;
    uint gy = uIndex / grid;
    
    float tile = 1.0 / grid;

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

    float3 baseColor = GAlbedo.Sample(LinearSampler, uv).rgb;

    float3 normal = GNormal.Sample(LinearSampler, uv).rgb;
    normal = normalize(normal * 2.0 - 1.0);

    float3 worldPos = GPosition.Sample(LinearSampler, uv).rgb;

    float4 material = GMaterial.Sample(LinearSampler, uv);
    float shininess = material.r * 128.0;
    float emission = material.g;

    float3 V = normalize(CameraPosition.xyz - worldPos);

    float3 color = float3(0, 0, 0);
    float3 ambient = float3(0, 0, 0);

    float3 finalColor = float3(0, 0, 0);
    
    int2 pixelCoord = int2(In.Position.xy);
    uint4 param = GParam.Load(int3(pixelCoord, 0));
    
    int sceneID = (int) param.x;
    int objectID = (int) param.y;
    int materialID = (int) param.z;
    
    switch (materialID)
    {
        case 0:
            finalColor = baseColor;
            break;
        
        case 1:
            for (int i = 0; i < LIGHT_MAX_COUNT; i++)
            {
                if (Lights[i].Enable == 0)
                {
                    continue;
                }
                ambient += baseColor * Lights[i].Ambient.rgb;
                color += ComputeLightContribution(
                    Lights[i],
                    normal,
                    V,
                    worldPos,
                    baseColor,
                    shininess,
                    i);
            }
            finalColor = ambient + color + emission;
            finalColor = pow(finalColor, 1.0 / 2.2);
            break;
            
        default:
            finalColor = float3(1, 0, 1);
            break;
    }
    return float4(finalColor, 1.0);
}
