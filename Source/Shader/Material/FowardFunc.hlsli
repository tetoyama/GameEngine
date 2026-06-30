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

#ifndef SHADOW_DEPTH_BIAS_HLSLI
#include "ShadowDepthBias.hlsli"
#endif

// ================= Texture =================
Texture2D BaseColorTex : register(t0);
SamplerState LinearSampler : register(s0);

// ================= Shadow =================
Texture2D ShadowMap : register(t7);
SamplerComparisonState ShadowSampler : register(s1);

// ================= Environment Map =================
Texture2D EnvironmentMap : register(t8);
SamplerState EnvSampler : register(s3);

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
    if ((Material.MaterialFlags & MATERIAL_FLAG_USE_DIFFUSE_TEXTURE) != 0)
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
    input.materialFlags = Material.MaterialFlags;
    
    BaseColorTex.GetDimensions(input.screenSize.x, input.screenSize.y);
    input.screenUV = In.Position.xy / float2(input.screenSize.x, input.screenSize.y);
    
    return input;
}

float SampleCascadePCF(float2 suvBase, float depth, float2 texelSize, float stepTexel, int radius);

float SampleShadowAtlasPCF(
    float2 uv,
    float depth,
    int tileIndex,
    ShadowPCFParams pcf)
{
    uint grid = (uint) ceil(sqrt((float) ShadowAtlasCount));
    float tile = 1.0 / grid;

    uint gx = tileIndex % grid;
    uint gy = tileIndex / grid;

    float2 tileMin = float2(gx, gy) * tile;
    float2 suvBase = tileMin + uv * tile;

    uint texW, texH;
    ShadowMap.GetDimensions(texW, texH);

    float2 texelSize = float2(1.0 / texW, 1.0 / texH);
    texelSize *= tile;

    int radius = max(pcf.KernelRadius, 0);
    return SampleCascadePCF(
        suvBase,
        depth,
        texelSize,
        pcf.StepTexel,
        radius
    );
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

    const float perspectiveViewDepth = sp.w;
    sp.xyz /= sp.w;

    float2 uv = sp.xy * 0.5 + 0.5;
    uv.y = 1.0 - uv.y;

    if (any(uv < 0.0) || any(uv > 1.0))
        return 1.0;

    // Directional keeps Param.w as legacy NDC bias.
    // Spot converts Param.w as world-distance bias for perspective depth.
    float bias = light.Param.w;
    if (light.LightType == LIGHT_TYPE_SPOT)
    {
        bias = ResolvePerspectiveShadowDepthBias(
            perspectiveViewDepth,
            light.Param.x,
            light.Param.w);
    }
    float depth = saturate(sp.z - bias);

    return SampleShadowAtlasPCF(uv, depth, lightIndex, pcf);
}

int SelectPointShadowFace(float3 direction)
{
    float3 absDir = abs(direction);

    if (absDir.x >= absDir.y && absDir.x >= absDir.z)
    {
        return (direction.x >= 0.0) ? 0 : 1;
    }

    if (absDir.y >= absDir.z)
    {
        return (direction.y >= 0.0) ? 2 : 3;
    }

    return (direction.z >= 0.0) ? 4 : 5;
}

float ShadowFactorPoint(
    float3 worldPos,
    int firstLightIdx,
    int faceCount,
    int atlasOffset,
    ShadowPCFParams pcf)
{
    if (firstLightIdx >= LIGHT_MAX_COUNT || faceCount <= 0)
        return 1.0;

    LIGHT firstFaceLight = Lights[firstLightIdx];
    float3 direction = worldPos - firstFaceLight.Position.xyz;
    int selectedFace = SelectPointShadowFace(direction);

    if (selectedFace >= faceCount)
        return 1.0;

    int faceLightIdx = firstLightIdx + selectedFace;
    if (faceLightIdx >= LIGHT_MAX_COUNT)
        return 1.0;

    LIGHT faceLight = Lights[faceLightIdx];
    if (!faceLight.Enable || !faceLight.CastShadow || faceLight.LightType != LIGHT_TYPE_POINT || faceLight.Dummy > -1)
        return 1.0;

    float4 sp = mul(float4(worldPos, 1.0), faceLight.LightView);
    sp = mul(sp, faceLight.LightProjection);

    if (sp.w <= 0.0)
        return 1.0;

    const float perspectiveViewDepth = sp.w;
    sp.xyz /= sp.w;

    float2 uv = sp.xy * 0.5 + 0.5;
    uv.y = 1.0 - uv.y;

    if (any(uv < 0.0) || any(uv > 1.0))
        return 1.0;

    const float bias = ResolvePerspectiveShadowDepthBias(
        perspectiveViewDepth,
        faceLight.Param.x,
        faceLight.Param.w);
    float depth = saturate(sp.z - bias);
    return SampleShadowAtlasPCF(uv, depth, atlasOffset + selectedFace, pcf);
}

// =====================================================
// CSM PCF サンプリングヘルパー
// =====================================================
float SampleCascadePCF(float2 suvBase, float depth, float2 texelSize, float stepTexel, int radius)
{
    float shadow = 0.0;
    int count = 0;
    [loop]
    for (int sy = -radius; sy <= radius; sy++)
    {
        [loop]
        for (int sx = -radius; sx <= radius; sx++)
        {
            shadow += ShadowMap.SampleCmpLevelZero(
                ShadowSampler,
                suvBase + float2(sx, sy) * texelSize * stepTexel,
                depth);
            count++;
        }
    }
    return shadow / max(count, 1);
}


float ShadowFactorCascades(
    float3 worldPos,
    int firstLightIdx,
    int cascadeCount,
    int atlasOffset,
    float receiverNdotL,
    ShadowPCFParams pcf)
{
    uint texW, texH;
    ShadowMap.GetDimensions(texW, texH);

    uint grid = (uint) ceil(sqrt((float) ShadowAtlasCount));
    float tile = 1.0 / grid;
    float2 texelSize = float2(1.0 / texW, 1.0 / texH) * tile;

    int radius = max(pcf.KernelRadius, 0);

    float finalShadow = 1.0;

    [loop]
    for (int c = 0; c < cascadeCount; c++)
    {
        int safeIdx = min(firstLightIdx + c, LIGHT_MAX_COUNT - 1);
        LIGHT cLight = Lights[safeIdx];

        float4 csp = mul(float4(worldPos, 1.0), cLight.LightView);
        csp = mul(csp, cLight.LightProjection);

        if (csp.w <= 0.0)
            continue;

        float3 ndc = csp.xyz / csp.w;
        float2 cuv = ndc.xy * 0.5 + 0.5;
        cuv.y = 1.0 - cuv.y;
        float cdepth = ndc.z;

        bool inUV = all(cuv >= 0.0) && all(cuv <= 1.0);
        if (!inUV)
            continue;

        float bias = ResolveOrthographicShadowDepthBias(
            cLight.Param.w,
            receiverNdotL);
        float depth = saturate(cdepth - bias);

        int tileIndex = atlasOffset + c;
        uint gx = tileIndex % grid;
        uint gy = tileIndex / grid;
        float2 tileMin = float2(gx, gy) * tile;
        float2 suvBase = tileMin + cuv * tile;

        float shadow = SampleCascadePCF(
            suvBase,
            depth,
            texelSize,
            pcf.StepTexel,
            radius
        );
        if (shadow >= 1.0 && c < cascadeCount - 1)
        {
            continue;
        }
        
        if (shadow < 1.0 && c > 0)
        {
            int3 loadCoord = int3((int) (suvBase.x * texW), (int) (suvBase.y * texH), 0);
            float rawDepth = ShadowMap.Load(loadCoord).r;

            float zScale = cLight.LightProjection[2][2];
            float deltaZ_ndc = abs(cdepth - rawDepth);
            float deltaZ_view = deltaZ_ndc / abs(zScale);
            float3 occluderPos = worldPos - cLight.Direction.xyz * deltaZ_view;

            LIGHT prevLight = Lights[safeIdx - 1];
            
            float4 prevSp = mul(float4(occluderPos, 1.0), prevLight.LightView);
            prevSp = mul(prevSp, prevLight.LightProjection);
            
            LIGHT firstLight = Lights[min(firstLightIdx, LIGHT_MAX_COUNT - 1)];
            float4 firstSp = mul(float4(occluderPos, 1.0), firstLight.LightView);
            firstSp = mul(firstSp, firstLight.LightProjection);
            float3 firstNdc = firstSp.xyz / firstSp.w;

            if (prevSp.w > 0.0)
            {
                float3 prevNdc = prevSp.xyz / prevSp.w;
                float2 prevUv = prevNdc.xy * 0.5 + 0.5;
                prevUv.y = 1.0 - prevUv.y;

                if (all(prevUv > 0.0) && all(prevUv < 1.0) && firstNdc.z > 0.0 && prevNdc.z < 1.0)
                {
                    continue;
                }
            }
        }

        finalShadow = min(finalShadow, shadow);
        
        if (0.0f >= finalShadow)
        {
            break;
        }
    }

    return finalShadow;
}


#endif
