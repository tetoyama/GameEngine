#ifndef FORWARD_FUNC_HLSLI
#define FORWARD_FUNC_HLSLI

#ifndef MATERIAL_DEFINE_HLSLI
#include "MaterialDefine.hlsli"
#endif

#ifndef COMMON_DEFINE_HLSLI
#include "../commonDefine.h"
#endif

#ifndef COMMON_HLSL
#include "../common.hlsl"
#endif

// ================= Texture =================
Texture2D BaseColorTex : register(t0);
SamplerState LinearSampler : register(s0);

// ================= Shadow =================
Texture2D ShadowMap : register(t7);
SamplerComparisonState ShadowSampler : register(s1);

// ============================================================================
// Forward : MaterialInput
// ============================================================================
MaterialInput GetMaterialInput(PS_IN In)
{
    MaterialInput input;
    input.uv = In.TexCoord;

    // ===== World =====
    input.worldPos = In.WorldPosition.xyz;
    input.viewDir = normalize(CameraPosition.xyz - input.worldPos);

    // ===== Material =====
    input.baseColor = Material.BaseColor;
    // Diffuse Texture を使うか
    if (Material.MaterialFlags & MATERIAL_FLAG_USE_DIFFUSE_TEXTURE != 0)
    {
        input.baseColor *= BaseColorTex.Sample(LinearSampler, In.TexCoord);
    }

    input.normal = normalize(In.Normal.xyz);
    
    input.Metallic = Material.Metallic;
    input.Roughness = Material.Roughness;
    input.AO = Material.AO;
    input.Emissive = float4(Material.EmissiveColor, Material.EmissiveIntensity);

    // ===== IDs =====
    input.materialID = ShaderID;
    input.objectID = ObjectID;
    input.sceneID = SceneID;
    
    BaseColorTex.GetDimensions(input.screenSize.x, input.screenSize.y);
    input.screenUV = In.Position.xy / float2(input.screenSize.x, input.screenSize.y);
    
    return input;
}

float ShadowFactor(
    float3 worldPos,
    LIGHT light,
    int lightIndex,
    ShadowPCFParams pcf)
{
    // ---- Light Space ----
    float4 sp = mul(float4(worldPos, 1.0), light.LightView);
    sp = mul(sp, light.LightProjection);

    if (sp.w <= 0.0)
        return 1.0;

    sp.xyz /= sp.w;

    float2 uv = sp.xy * 0.5 + 0.5;
    uv.y = 1.0 - uv.y;

    if (any(uv < 0.0) || any(uv > 1.0))
        return 1.0;

    float depth = saturate(sp.z - DEPTH_BIAS_CONSTANT);

    // ---- Atlas ----
    uint grid = (uint) ceil(sqrt((float) ShadowAtlasCount));
    float tile = 1.0 / grid;

    uint gx = lightIndex % grid;
    uint gy = lightIndex / grid;

    float2 tileMin = float2(gx, gy) * tile;
    float2 suvBase = tileMin + uv * tile;

    // ---- テクセルサイズ（1ピクセル）----
    float2 texelSize;
    ShadowMap.GetDimensions(texelSize.x, texelSize.y);
    texelSize = 1.0 / texelSize;

    texelSize *= tile; // アトラス対応

    // ---- PCF ----
    float shadow = 0.0;
    int radius = max(pcf.KernelRadius, 0);
    int count = 0;

    [loop]
    for (int y = -radius; y <= radius; y++)
    {
        [loop]
        for (int x = -radius; x <= radius; x++)
        {
            float2 offset =
                float2(x, y) * texelSize * pcf.StepTexel;

            shadow += ShadowMap.SampleCmpLevelZero(
                ShadowSampler,
                suvBase + offset,
                depth);

            count++;
        }
    }

    return shadow / max(count, 1);
}

// =====================================================
// CSM (Cascaded Shadow Maps) シャドウファクター (Forward)
// =====================================================
float ShadowFactorCSM(
    float3 worldPos,
    ShadowPCFParams pcf)
{
    float4 viewPos = mul(float4(worldPos, 1.0), View);
    float viewDepth = viewPos.z;

    int cascade = DIRECTIONAL_CSM_CASCADE_COUNT - 1;
    [unroll]
    for (int c = 0; c < DIRECTIONAL_CSM_CASCADE_COUNT; c++)
    {
        float splitDepth;
        if (c < 4)
            splitDepth = CsmSplitDepths[0][c];
        else
            splitDepth = CsmSplitDepths[1][c - 4];

        if (viewDepth < splitDepth)
        {
            cascade = c;
            break;
        }
    }

    float4 sp = mul(float4(worldPos, 1.0), CsmViews[cascade]);
    sp = mul(sp, CsmProjections[cascade]);

    if (sp.w <= 0.0)
        return 1.0;

    sp.xyz /= sp.w;

    float2 uv = sp.xy * 0.5 + 0.5;
    uv.y = 1.0 - uv.y;

    if (any(uv < 0.0) || any(uv > 1.0))
        return 1.0;

    float depth = saturate(sp.z - DEPTH_BIAS_CONSTANT);

    int tileIndex = CsmAtlasOffset + cascade;
    uint grid = (uint) ceil(sqrt((float) ShadowAtlasCount));
    float tile = 1.0 / grid;

    uint gx = tileIndex % grid;
    uint gy = tileIndex / grid;

    float2 tileMin = float2(gx, gy) * tile;
    float2 suvBase = tileMin + uv * tile;

    float2 texelSize;
    ShadowMap.GetDimensions(texelSize.x, texelSize.y);
    texelSize = 1.0 / texelSize;
    texelSize *= tile;

    float shadow = 0.0;
    int radius = max(pcf.KernelRadius, 0);
    int count = 0;

    [loop]
    for (int y = -radius; y <= radius; y++)
    {
        [loop]
        for (int x = -radius; x <= radius; x++)
        {
            float2 offset = float2(x, y) * texelSize * pcf.StepTexel;
            shadow += ShadowMap.SampleCmpLevelZero(ShadowSampler, suvBase + offset, depth);
            count++;
        }
    }

    return shadow / max(count, 1);
}


#endif
