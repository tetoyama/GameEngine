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

Texture2D BaseColorTex : register(t0);
SamplerState LinearSampler : register(s0);

Texture2D ShadowMap : register(t7);
SamplerComparisonState ShadowSampler : register(s1);

Texture2D EnvironmentMap : register(t8);
SamplerState EnvSampler : register(s3);

MaterialInput GetMaterialInput(PS_IN In)
{
    MaterialInput input;
    input.uv = In.TexCoord;
    input.worldPos = In.WorldPosition.xyz;
    input.viewDir = normalize(CameraPosition.xyz - input.worldPos);

    input.baseColor = Material.BaseColor;
    if ((Material.MaterialFlags & MATERIAL_FLAG_USE_DIFFUSE_TEXTURE) != 0)
        input.baseColor *= BaseColorTex.Sample(LinearSampler, In.TexCoord);

    input.normal = normalize(In.Normal.xyz);
    input.Metallic = Material.Metallic;
    input.Roughness = Material.Roughness;
    input.AO = Material.AO;
    input.Emissive = float4(Material.EmissiveColor, Material.EmissiveIntensity);

    input.materialID = ShaderID;
    input.objectID = ObjectID;
    input.sceneID = SceneID;
    input.materialFlags = Material.MaterialFlags;

    BaseColorTex.GetDimensions(input.screenSize.x, input.screenSize.y);
    input.screenUV = In.Position.xy / float2(input.screenSize.x, input.screenSize.y);
    return input;
}

int ResolveShadowAtlasTileForEntry(int lightEntryIndex)
{
    int activeEntryCount = clamp(ActiveLightCount, 0, LIGHT_MAX_COUNT);
    int endEntry = clamp(lightEntryIndex, 0, activeEntryCount);
    int tileIndex = 0;

    [loop]
    for (int entryIndex = 0; entryIndex < endEntry; ++entryIndex)
    {
        LIGHT entry = Lights[entryIndex];
        if (entry.Enable != 0 && entry.CastShadow != 0)
            ++tileIndex;
    }

    return tileIndex;
}

float SampleCascadePCF(float2 suvBase, float depth, float2 texelSize, float stepTexel, int radius);

float SampleShadowAtlasPCF(
    float2 uv,
    float depth,
    int tileIndex,
    ShadowPCFParams pcf)
{
    uint atlasCount = max((uint)ShadowAtlasCount, 1u);
    uint grid = max((uint)ceil(sqrt((float)atlasCount)), 1u);
    float tile = 1.0 / grid;

    if (tileIndex < 0 || tileIndex >= (int)atlasCount)
        return 1.0;

    uint safeTileIndex = (uint)tileIndex;
    uint gx = safeTileIndex % grid;
    uint gy = safeTileIndex / grid;

    uint texW, texH;
    ShadowMap.GetDimensions(texW, texH);

    float2 atlasTexelSize = float2(1.0 / texW, 1.0 / texH);
    float2 tileMin = float2(gx, gy) * tile;
    float2 tileMax = tileMin + tile;
    float2 sampleMin = tileMin + atlasTexelSize * 0.5;
    float2 sampleMax = tileMax - atlasTexelSize * 0.5;
    float2 suvBase = clamp(
        tileMin + saturate(uv) * tile,
        sampleMin,
        sampleMax);

    float shadow = 0.0;
    int count = 0;
    int radius = max(pcf.KernelRadius, 0);

    [loop]
    for (int sy = -radius; sy <= radius; ++sy)
    {
        [loop]
        for (int sx = -radius; sx <= radius; ++sx)
        {
            float2 sampleUV = clamp(
                suvBase + float2(sx, sy) * atlasTexelSize * pcf.StepTexel,
                sampleMin,
                sampleMax);
            shadow += ShadowMap.SampleCmpLevelZero(
                ShadowSampler,
                sampleUV,
                depth);
            ++count;
        }
    }

    return shadow / max(count, 1);
}

float ShadowFactor(
    float3 worldPos,
    LIGHT light,
    int lightEntryIndex,
    float receiverNdotL,
    ShadowPCFParams pcf)
{
    float4 sp = mul(float4(worldPos, 1.0), light.LightView);
    sp = mul(sp, light.LightProjection);
    if (sp.w <= 0.0)
        return 1.0;

    sp.xyz /= sp.w;

    float2 uv = sp.xy * 0.5 + 0.5;
    uv.y = 1.0 - uv.y;
    if (any(uv < 0.0) || any(uv > 1.0))
        return 1.0;

    float depth;
    if (light.LightType == LIGHT_TYPE_SPOT)
    {
        float radialDistance = length(worldPos - light.Position.xyz);
        float bias = ResolveRadialShadowDepthBias(
            light.Param.x,
            light.Param.w,
            receiverNdotL);
        depth = saturate(radialDistance / max(light.Param.x, 0.001f) - bias);
    }
    else
    {
        depth = saturate(sp.z - light.Param.w);
    }

    int tileIndex = ResolveShadowAtlasTileForEntry(lightEntryIndex);
    return SampleShadowAtlasPCF(uv, depth, tileIndex, pcf);
}

int SelectPointShadowFace(float3 direction)
{
    float3 absDir = abs(direction);

    if (absDir.x >= absDir.y && absDir.x >= absDir.z)
        return direction.x >= 0.0 ? 0 : 1;

    if (absDir.y >= absDir.z)
        return direction.y >= 0.0 ? 2 : 3;

    return direction.z >= 0.0 ? 4 : 5;
}

float ShadowFactorPoint(
    float3 worldPos,
    int firstLightIdx,
    int faceCount,
    float receiverNdotL,
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
    if (!faceLight.Enable || !faceLight.CastShadow ||
        faceLight.LightType != LIGHT_TYPE_POINT || faceLight.Dummy > -1)
        return 1.0;

    float4 sp = mul(float4(worldPos, 1.0), faceLight.LightView);
    sp = mul(sp, faceLight.LightProjection);
    if (sp.w <= 0.0)
        return 1.0;

    sp.xyz /= sp.w;

    float2 uv = sp.xy * 0.5 + 0.5;
    uv.y = 1.0 - uv.y;
    if (any(uv < 0.0) || any(uv > 1.0))
        return 1.0;

    float radialDistance = length(direction);
    float bias = ResolveRadialShadowDepthBias(
        faceLight.Param.x,
        faceLight.Param.w,
        receiverNdotL);
    float depth = saturate(
        radialDistance / max(faceLight.Param.x, 0.001f) - bias);

    int tileIndex = ResolveShadowAtlasTileForEntry(faceLightIdx);
    return SampleShadowAtlasPCF(uv, depth, tileIndex, pcf);
}

float SampleCascadePCF(float2 suvBase, float depth, float2 texelSize, float stepTexel, int radius)
{
    float shadow = 0.0;
    int count = 0;

    [loop]
    for (int sy = -radius; sy <= radius; ++sy)
    {
        [loop]
        for (int sx = -radius; sx <= radius; ++sx)
        {
            shadow += ShadowMap.SampleCmpLevelZero(
                ShadowSampler,
                suvBase + float2(sx, sy) * texelSize * stepTexel,
                depth);
            ++count;
        }
    }

    return shadow / max(count, 1);
}

float ShadowFactorCascades(
    float3 worldPos,
    int firstLightIdx,
    int cascadeCount,
    float receiverNdotL,
    ShadowPCFParams pcf)
{
    uint texW, texH;
    ShadowMap.GetDimensions(texW, texH);

    uint grid = max((uint)ceil(sqrt((float)max(ShadowAtlasCount, 1))), 1u);
    float tile = 1.0 / grid;
    float2 texelSize = float2(1.0 / texW, 1.0 / texH) * tile;
    int radius = max(pcf.KernelRadius, 0);
    float finalShadow = 1.0;

    [loop]
    for (int c = 0; c < cascadeCount; ++c)
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

        if (any(cuv < 0.0) || any(cuv > 1.0))
            continue;

        float bias = ResolveOrthographicShadowDepthBias(cLight.Param.w, receiverNdotL);
        float depth = saturate(cdepth - bias);

        int tileIndex = ResolveShadowAtlasTileForEntry(safeIdx);
        if (tileIndex < 0 || tileIndex >= ShadowAtlasCount)
            continue;

        uint gx = tileIndex % grid;
        uint gy = tileIndex / grid;
        float2 tileMin = float2(gx, gy) * tile;
        float2 suvBase = tileMin + cuv * tile;

        float shadow = SampleCascadePCF(
            suvBase,
            depth,
            texelSize,
            pcf.StepTexel,
            radius);

        if (shadow >= 1.0 && c < cascadeCount - 1)
            continue;

        if (shadow < 1.0 && c > 0)
        {
            int3 loadCoord = int3((int)(suvBase.x * texW), (int)(suvBase.y * texH), 0);
            float rawDepth = ShadowMap.Load(loadCoord).r;
            float zScale = cLight.LightProjection[2][2];
            float deltaZ_ndc = abs(cdepth - rawDepth);
            float deltaZ_view = deltaZ_ndc / max(abs(zScale), 0.000001f);
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

                if (all(prevUv > 0.0) && all(prevUv < 1.0) &&
                    firstNdc.z > 0.0 && prevNdc.z < 1.0)
                    continue;
            }
        }

        finalShadow = min(finalShadow, shadow);
        if (finalShadow <= 0.0f)
            break;
    }

    return finalShadow;
}

#endif