#ifndef DEFERRED_FUNC_HLSLI
#define DEFERRED_FUNC_HLSLI

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

// ================= GBuffer =================
// ライティングパスで参照する GBuffer 群
Texture2D GAlbedo : register(t0);
Texture2D GNormal : register(t1);
Texture2D GPosition : register(t2);
Texture2D GMaterial : register(t3);
Texture2D GEmissive: register(t4);
Texture2D<uint4> GParam : register(t5);
SamplerState LinearSampler : register(s0);
SamplerState PointSampler : register(s2);

// ================= Shadow =================
Texture2D ShadowMap : register(t6);
SamplerComparisonState ShadowSampler : register(s1);

#ifndef SHADOW_ATLAS_SAMPLING_HLSLI
#include "ShadowAtlasSampling.hlsli"
#endif

// ================= Environment Map =================
Texture2D EnvironmentMap : register(t7);
SamplerState EnvSampler : register(s3);

// ================= Implement =================
// GBuffer全体を読まずにmaterialIDだけを取得する
// (マテリアル別パスでの早期discard判定用)
uint GetMaterialID(PS_IN In)
{
    uint textureWidth, textureHeight;
    GParam.GetDimensions(textureWidth, textureHeight);
    int2 pixelCoord = int2(In.TexCoord.x * textureWidth, In.TexCoord.y * textureHeight);
    return GParam.Load(int3(pixelCoord, 0)).z;
}

// GBuffer からマテリアル計算用の入力を復元する
MaterialInput GetMaterialInput(PS_IN In)
{
    MaterialInput input;
    input.uv = In.TexCoord;

    input.baseColor =
        float4(GAlbedo.Sample(PointSampler, input.uv).rgb, 1);

    input.normal =
        normalize(GNormal.Sample(PointSampler, input.uv).rgb * 2.0 - 1.0);

    uint textureWidth, textureHeight;
    GPosition.GetDimensions(textureWidth, textureHeight);
    int2 pixelCoord = int2(In.TexCoord.x * textureWidth, In.TexCoord.y * textureHeight);

    float3 worldPos = GPosition.Load(int3(pixelCoord, 0)).xyz;
    input.worldPos = worldPos;
    input.viewDir = normalize(CameraPosition.xyz - worldPos);

    float4 mat = GMaterial.Sample(PointSampler, input.uv);
    input.Metallic = mat.r;
    input.Roughness = mat.g;
    input.AO = mat.b;
    input.Emissive = GEmissive.Sample(PointSampler, input.uv);

    uint4 param = GParam.Load(int3(pixelCoord, 0));
    input.sceneID = param.x;
    input.objectID = param.y;
    input.materialID = param.z;
    input.materialFlags = param.w;

    GAlbedo.GetDimensions(input.screenSize.x, input.screenSize.y);
    input.screenUV = In.TexCoord;

    return input;
}

float ShadowFactorCascades(
    float3 worldPos,
    int firstLightIdx,
    int cascadeCount,
    int atlasOffset,
    ShadowPCFParams pcf)
{
    if (ShadowAtlasCount <= 0)
        return 1.0;

    uint texW, texH;
    ShadowMap.GetDimensions(texW, texH);

    const uint grid = max((uint) ceil(sqrt((float) ShadowAtlasCount)), 1u);
    const float tileSize = 1.0 / grid;
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

        if (any(cuv < 0.0) || any(cuv > 1.0))
            continue;

        float depth = saturate(cdepth - cLight.Param.w);
        int tileIndex = atlasOffset + c;
        if (tileIndex < 0 || tileIndex >= ShadowAtlasCount)
            continue;

        float shadow = SampleShadowAtlasPCF(
            cuv,
            depth,
            tileIndex,
            pcf);

        // Keep the existing cascade fallback. A fully lit result may mean the
        // caster was clipped from a finer cascade, so test the next cascade.
        if (shadow >= 1.0 && c < cascadeCount - 1)
            continue;

        if (shadow < 1.0 && c > 0)
        {
            uint tileX = tileIndex % grid;
            uint tileY = tileIndex / grid;
            float2 tileMin = float2(tileX, tileY) * tileSize;
            float2 atlasUv = tileMin + cuv * tileSize;
            int2 maxLoadCoord = int2((int) texW - 1, (int) texH - 1);
            int2 loadCoord = clamp(
                int2(atlasUv * float2(texW, texH)),
                int2(0, 0),
                maxLoadCoord);
            float rawDepth = ShadowMap.Load(int3(loadCoord, 0)).r;

            float zScale = cLight.LightProjection[2][2];
            float safeZScale = max(abs(zScale), 0.000001);
            float deltaZView = abs(cdepth - rawDepth) / safeZScale;
            float3 occluderPos =
                worldPos - cLight.Direction.xyz * deltaZView;

            LIGHT prevLight = Lights[safeIdx - 1];
            float4 prevSp = mul(float4(occluderPos, 1.0), prevLight.LightView);
            prevSp = mul(prevSp, prevLight.LightProjection);

            LIGHT firstLight = Lights[min(firstLightIdx, LIGHT_MAX_COUNT - 1)];
            float4 firstSp = mul(float4(occluderPos, 1.0), firstLight.LightView);
            firstSp = mul(firstSp, firstLight.LightProjection);

            if (prevSp.w > 0.0 && firstSp.w > 0.0)
            {
                float3 prevNdc = prevSp.xyz / prevSp.w;
                float2 prevUv = prevNdc.xy * 0.5 + 0.5;
                prevUv.y = 1.0 - prevUv.y;
                float3 firstNdc = firstSp.xyz / firstSp.w;

                if (all(prevUv > 0.0) && all(prevUv < 1.0) &&
                    firstNdc.z > 0.0 && prevNdc.z < 1.0)
                {
                    continue;
                }
            }
        }

        finalShadow = min(finalShadow, shadow);
        if (finalShadow <= 0.0)
            break;
    }

    return finalShadow;
}

float ShadowFactor(
    float3 worldPos,
    LIGHT light,
    int lightIndex,
    ShadowPCFParams pcf)
{
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

    // Legacy mode keeps Param.w as projected-depth bias. World-space mode
    // applies its receiver offset before this function and mirrors Param.w=0.
    float bias = light.Param.w;
    if (light.LightType == LIGHT_TYPE_SPOT)
    {
        bias = ResolvePerspectiveShadowDepthBias(
            perspectiveViewDepth,
            light.Param.x,
            light.Param.w);
    }

    return SampleShadowAtlasPCF(
        uv,
        saturate(sp.z - bias),
        lightIndex,
        pcf);
}

int SelectPointShadowFace(float3 direction)
{
    float3 absDir = abs(direction);

    if (absDir.x >= absDir.y && absDir.x >= absDir.z)
        return (direction.x >= 0.0) ? 0 : 1;

    if (absDir.y >= absDir.z)
        return (direction.y >= 0.0) ? 2 : 3;

    return (direction.z >= 0.0) ? 4 : 5;
}

float ShadowFactorPoint(
    float3 worldPos,
    int firstLightIdx,
    int faceCount,
    int atlasOffset,
    ShadowPCFParams pcf)
{
    if (firstLightIdx < 0 || firstLightIdx >= LIGHT_MAX_COUNT || faceCount <= 0)
        return 1.0;

    LIGHT firstFaceLight = Lights[firstLightIdx];
    int selectedFace = SelectPointShadowFace(
        worldPos - firstFaceLight.Position.xyz);

    if (selectedFace < 0 || selectedFace >= faceCount)
        return 1.0;

    int faceLightIdx = firstLightIdx + selectedFace;
    if (faceLightIdx >= LIGHT_MAX_COUNT)
        return 1.0;

    LIGHT faceLight = Lights[faceLightIdx];
    if (!faceLight.Enable ||
        !faceLight.CastShadow ||
        faceLight.LightType != LIGHT_TYPE_POINT ||
        faceLight.Dummy > -1)
    {
        return 1.0;
    }

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

    return SampleShadowAtlasPCF(
        uv,
        saturate(sp.z - bias),
        atlasOffset + selectedFace,
        pcf);
}

#endif
