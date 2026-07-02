#ifndef MATERIAL_FUNC_HLSLI
#define MATERIAL_FUNC_HLSLI

#ifndef MATERIAL_DEFINE_HLSLI

#include "MaterialDefine.hlsli"

#ifdef FORWARD_FUNC_HLSLI
#include "FowardFunc.hlsli"
#else
#include "DeferredFunc.hlsli"
#endif//FORWARD_FUNC_HLSLI

#endif //MATERIAL_DEFINE_HLSLI

#ifndef COMMON_DEFINE_HLSLI
#include "../commonDefine.h"
#endif

#ifndef COMMON_HLSL
#include "../common.hlsl"
#endif

// ===== BRDF =====
float3 FresnelSchlick(float cosTheta, float3 F0)
{
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

float SchlickGGX(float NdotV, float roughness)
{
    float a = roughness;
    float k = (a * a + 1.0) / 8.0;
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

// PR #46で正常だったCSM専用PCF処理。
// CSMについては共通Atlas PCFへ統合せず、当時のTexel幅とFallbackを維持する。
float SampleCascadePCFPrevious(
    float2 suvBase,
    float depth,
    float2 texelSize,
    float stepTexel,
    int radius)
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

// PR #46時点のCSM選択・Fallback・合成処理を維持する。
// Biasのみ固定値から受光面角度対応へ変更し、Cascade選択とPCF精度には触れない。
float ShadowFactorCascadesPrevious(
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

        // 旧SceneのParam.wを基準値として維持しつつ、
        // Grazing面ではSlope Scaleを加えてCSM Acneを抑制する。
        float bias = ResolveOrthographicShadowDepthBias(
            cLight.Param.w,
            receiverNdotL);
        float depth = saturate(cdepth - bias);

        int tileIndex = atlasOffset + c;
        uint gx = tileIndex % grid;
        uint gy = tileIndex / grid;
        float2 tileMin = float2(gx, gy) * tile;
        float2 suvBase = tileMin + cuv * tile;

        float shadow = SampleCascadePCFPrevious(
            suvBase,
            depth,
            texelSize,
            pcf.StepTexel,
            radius);

        if (shadow >= 1.0 && c < cascadeCount - 1)
            continue;

        if (shadow < 1.0 && c > 0)
        {
            int3 loadCoord = int3(
                (int) (suvBase.x * texW),
                (int) (suvBase.y * texH),
                0);
            float rawDepth = ShadowMap.Load(loadCoord).r;

            float zScale = cLight.LightProjection[2][2];
            float deltaZ_ndc = abs(cdepth - rawDepth);
            float deltaZ_view = deltaZ_ndc / abs(zScale);
            float3 occluderPos =
                worldPos - cLight.Direction.xyz * deltaZ_view;

            LIGHT prevLight = Lights[safeIdx - 1];
            float4 prevSp = mul(
                float4(occluderPos, 1.0),
                prevLight.LightView);
            prevSp = mul(prevSp, prevLight.LightProjection);

            LIGHT firstLight = Lights[min(firstLightIdx, LIGHT_MAX_COUNT - 1)];
            float4 firstSp = mul(
                float4(occluderPos, 1.0),
                firstLight.LightView);
            firstSp = mul(firstSp, firstLight.LightProjection);
            float3 firstNdc = firstSp.xyz / firstSp.w;

            if (prevSp.w > 0.0)
            {
                float3 prevNdc = prevSp.xyz / prevSp.w;
                float2 prevUv = prevNdc.xy * 0.5 + 0.5;
                prevUv.y = 1.0 - prevUv.y;

                if (all(prevUv > 0.0) &&
                    all(prevUv < 1.0) &&
                    firstNdc.z > 0.0 &&
                    prevNdc.z < 1.0)
                {
                    continue;
                }
            }
        }

        finalShadow = min(finalShadow, shadow);
        if (0.0f >= finalShadow)
            break;
    }

    return finalShadow;
}

int ResolvePackedLightEntrySpan(LIGHT light, int firstEntryIndex, int activeEntryCount)
{
    int remainingEntries = max(activeEntryCount - firstEntryIndex, 1);
    int span = 1;

    if (light.LightType == LIGHT_TYPE_DIRECTIONAL_CSM && light.Dummy == 1)
    {
        span = max((int) round(light.Position.w), 1);
    }
    else if (light.LightType == LIGHT_TYPE_POINT && light.Dummy == -1)
    {
        span = max((int) round(light.Position.w), 1);
    }

    return min(span, remainingEntries);
}

float3 ResolvePointShadowFaceDirection(int faceIndex)
{
    if (faceIndex == 0) return float3( 1.0,  0.0,  0.0);
    if (faceIndex == 1) return float3(-1.0,  0.0,  0.0);
    if (faceIndex == 2) return float3( 0.0,  1.0,  0.0);
    if (faceIndex == 3) return float3( 0.0, -1.0,  0.0);
    if (faceIndex == 4) return float3( 0.0,  0.0,  1.0);
    return float3(0.0, 0.0, -1.0);
}

float ResolvePointReceiverFaceAlignment(
    float3 worldPos,
    float3 lightPosition,
    float3 receiverNormal)
{
    const float3 lightToReceiver = worldPos - lightPosition;
    const int selectedFace = SelectPointShadowFace(lightToReceiver);
    const float3 faceDirection = ResolvePointShadowFaceDirection(selectedFace);

    return saturate(abs(dot(normalize(receiverNormal), faceDirection)));
}

bool ShouldEvaluateShadow(LIGHT light)
{
    if (light.CastShadow == 0)
        return false;

    if ((LightingDebugFlags & LIGHTING_DEBUG_FLAG_DISABLE_SHADOWS) != 0u)
        return false;

    if (light.LightType == LIGHT_TYPE_DIRECTIONAL_CSM &&
        (LightingDebugFlags & LIGHTING_DEBUG_FLAG_DISABLE_CSM_SHADOWS) != 0u)
        return false;

    if (light.LightType == LIGHT_TYPE_POINT &&
        (LightingDebugFlags & LIGHTING_DEBUG_FLAG_DISABLE_POINT_SHADOWS) != 0u)
        return false;

    return true;
}

LightingResult ComputeLightingFromMaterialInput(MaterialInput input, ShadowPCFParams shadowParam)
{
    LightingResult result = (LightingResult) 0;

    float3 N = normalize(input.normal);
    float3 V = normalize(CameraPosition.xyz - input.worldPos);

    float roughness = saturate(input.Roughness);
    float metallic = saturate(input.Metallic);

    float3 F0 = lerp(float3(0.04, 0.04, 0.04), float3(1.0, 1.0, 1.0), metallic);

    int activeEntryCount = clamp(ActiveLightCount, 0, LIGHT_MAX_COUNT);
    int entryIndex = 0;
    int shadowAtlasOffset = 0;

    [loop]
    while (entryIndex < activeEntryCount)
    {
        int currentEntryIndex = entryIndex;
        LIGHT light = Lights[currentEntryIndex];
        int entrySpan = ResolvePackedLightEntrySpan(
            light,
            currentEntryIndex,
            activeEntryCount);
        int currentShadowAtlasOffset = shadowAtlasOffset;

        entryIndex += entrySpan;
        if (light.CastShadow != 0)
            shadowAtlasOffset += entrySpan;

        if (light.Enable == 0)
            continue;

        if (light.LightType == LIGHT_TYPE_POINT && light.Dummy < -1)
            continue;
        if (light.LightType == LIGHT_TYPE_DIRECTIONAL_CSM && light.Dummy > 1)
            continue;

        float3 L;
        float attenuation = 1.0;

        if (light.LightType == LIGHT_TYPE_DIRECTIONAL ||
            light.LightType == LIGHT_TYPE_DIRECTIONAL_CSM)
        {
            L = normalize(-light.Direction.xyz);
        }
        else
        {
            float3 toL = light.Position.xyz - input.worldPos;
            float dist = length(toL);

            L = toL / max(dist, 0.001);
            attenuation = saturate(1.0 - dist / max(light.Param.x, 0.001));

            if (light.LightType == LIGHT_TYPE_SPOT)
            {
                float3 spotDir = normalize(-light.Direction.xyz);
                float cosTheta = dot(L, spotDir);
                float innerCos = cos(radians(light.Param.y));
                float outerCos = cos(radians(light.Param.z));
                attenuation *= saturate(
                    (cosTheta - outerCos) /
                    max(innerCos - outerCos, 0.001));
            }
        }

        float NdotL = saturate(dot(N, L));
        float NdotV = saturate(dot(N, V));

        if (NdotL <= 0)
            continue;

        if (attenuation <= 0.0f)
        {
            result.ambient += light.Ambient.rgb;
            continue;
        }

        float shadow = 1.0;

        if (ShouldEvaluateShadow(light))
        {
            const float3 shadowWorldPos = ApplyShadowReceiverBias(
                input.worldPos,
                N,
                L,
                NdotL,
                light);

            if (light.LightType == LIGHT_TYPE_DIRECTIONAL_CSM && light.Dummy == 1)
            {
                shadow = ShadowFactorCascadesPrevious(
                    shadowWorldPos,
                    currentEntryIndex,
                    entrySpan,
                    currentShadowAtlasOffset,
                    NdotL,
                    shadowParam);
            }
            else if (light.LightType == LIGHT_TYPE_POINT && light.Dummy == -1)
            {
                const float receiverFaceAlignment =
                    ResolvePointReceiverFaceAlignment(
                        input.worldPos,
                        light.Position.xyz,
                        N);

                shadow = ShadowFactorPoint(
                    shadowWorldPos,
                    currentEntryIndex,
                    entrySpan,
                    receiverFaceAlignment,
                    shadowParam);
            }
            else
            {
                shadow = ShadowFactor(
                    shadowWorldPos,
                    light,
                    currentEntryIndex,
                    NdotL,
                    shadowParam);
            }
        }

        float toonShadow = lerp(0.1, 1.0, shadow);

        float3 diffuse =
            light.Diffuse.rgb *
            attenuation *
            NdotL *
            toonShadow;

        float3 H = normalize(V + L);
        float NdotH = saturate(dot(N, H));
        float3 F = FresnelSchlick(saturate(dot(V, H)), F0);
        float G = G_Smith(N, V, L, roughness);
        float D = D_GTR2(NdotH, roughness);

        float3 specBRDF =
            (D * G * F) /
            max(4.0 * NdotL * NdotV, 0.001);

        float specularShadow =
            lerp(1.0, shadow, saturate(1.0 - roughness));

        float3 specular =
            specBRDF *
            light.Diffuse.rgb *
            attenuation *
            NdotL *
            specularShadow;

        result.diffuse += saturate(diffuse);
        result.specular += specular;
        result.ambient += light.Ambient.rgb;
    }

    return result;
}

float Quantize4(float v)
{
    v = saturate(v);
    v = floor(v * 4.0) / 3.0;
    v = saturate(saturate(saturate(v - 0.1) + 0.2) - 0.1);
    return v;
}

#endif
