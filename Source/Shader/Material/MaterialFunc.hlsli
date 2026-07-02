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

// CSMは高精度Cascadeから順番に評価する。
// 現在のCascadeが完全にLitの場合だけ後段Cascadeへフォールバックし、
// 後段で見つかったShadowはOccluderを上位Cascadeへ再投影して妥当性を確認する。
// 最初に妥当と判定されたShadowを即時採用し、低解像度Cascadeによる上書きを防ぐ。
float ShadowFactorCascadesHighPrecisionFallback(
    float3 worldPos,
    int firstLightIdx,
    int cascadeCount,
    float receiverNdotL,
    ShadowPCFParams pcf)
{
    if (firstLightIdx < 0 ||
        firstLightIdx >= LIGHT_MAX_COUNT ||
        cascadeCount <= 0)
    {
        return 1.0;
    }

    uint texW, texH;
    ShadowMap.GetDimensions(texW, texH);

    const int safeCascadeCount = min(
        cascadeCount,
        LIGHT_MAX_COUNT - firstLightIdx);

    [loop]
    for (int c = 0; c < safeCascadeCount; ++c)
    {
        const int safeIdx = firstLightIdx + c;
        LIGHT cLight = Lights[safeIdx];

        float4 csp = mul(float4(worldPos, 1.0), cLight.LightView);
        csp = mul(csp, cLight.LightProjection);

        if (csp.w <= 0.0)
            continue;

        float3 ndc = csp.xyz / csp.w;
        float2 cuv = ndc.xy * 0.5 + 0.5;
        cuv.y = 1.0 - cuv.y;
        float cdepth = ndc.z;

        // CSMのDepth Clamp契約を維持するため、Receiver選択はXY範囲で行う。
        if (any(cuv < 0.0) || any(cuv > 1.0))
            continue;

        const float bias = ResolveOrthographicShadowDepthBias(
            cLight.Param.w,
            receiverNdotL);
        const float depth = saturate(cdepth - bias);

        const int tileIndex = ResolveShadowAtlasTileForEntry(safeIdx);
        float2 atlasTexelSize;
        float2 sampleMin;
        float2 sampleMax;
        const float2 suvBase = ResolveShadowAtlasSampleBase(
            cuv,
            tileIndex,
            atlasTexelSize,
            sampleMin,
            sampleMax);
        const float shadow = SampleShadowAtlasPCFResolved(
            suvBase,
            depth,
            atlasTexelSize,
            sampleMin,
            sampleMax,
            pcf);

        // このCascadeで影が見つからない場合は、Near ClipなどでCasterが
        // 欠落している可能性があるため、後段Cascadeを確認する。
        if (shadow >= 1.0f)
            continue;

        if (c > 0)
        {
            const int2 safeLoadCoord = clamp(
                int2(suvBase * float2(texW, texH)),
                int2(0, 0),
                int2((int) texW - 1, (int) texH - 1));
            const float rawDepth = ShadowMap.Load(int3(safeLoadCoord, 0)).r;
            const float zScale = cLight.LightProjection[2][2];
            const float deltaZ_ndc = abs(cdepth - rawDepth);
            const float deltaZ_view =
                deltaZ_ndc / max(abs(zScale), 0.000001f);
            const float3 occluderPos =
                worldPos - cLight.Direction.xyz * deltaZ_view;

            LIGHT prevLight = Lights[safeIdx - 1];
            float4 prevSp = mul(
                float4(occluderPos, 1.0),
                prevLight.LightView);
            prevSp = mul(prevSp, prevLight.LightProjection);

            LIGHT firstLight = Lights[firstLightIdx];
            float4 firstSp = mul(
                float4(occluderPos, 1.0),
                firstLight.LightView);
            firstSp = mul(firstSp, firstLight.LightProjection);

            if (prevSp.w > 0.0 && firstSp.w > 0.0)
            {
                const float3 prevNdc = prevSp.xyz / prevSp.w;
                float2 prevUv = prevNdc.xy * 0.5 + 0.5;
                prevUv.y = 1.0 - prevUv.y;
                const float3 firstNdc = firstSp.xyz / firstSp.w;

                const bool occluderBelongsToHigherPrecisionCascade =
                    all(prevUv > 0.0) &&
                    all(prevUv < 1.0) &&
                    firstNdc.z > 0.0 &&
                    prevNdc.z < 1.0;

                if (occluderBelongsToHigherPrecisionCascade)
                {
                    // 後段CascadeのShadowを誤って採用せず、さらに後段を確認する。
                    continue;
                }
            }
        }

        // NearからFarへ評価しているため、ここが最も高精度な妥当Shadow。
        return shadow;
    }

    return 1.0;
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

    // Perspective shadow acne is governed by the receiver slope relative to
    // the selected face camera, not by NdotL to the radial light direction.
    // A horizontal floor viewed by a horizontal cubemap face is a grazing
    // receiver even while its radial NdotL remains non-zero.
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

    [loop]
    while (entryIndex < activeEntryCount)
    {
        int currentEntryIndex = entryIndex;
        LIGHT light = Lights[currentEntryIndex];
        int entrySpan = ResolvePackedLightEntrySpan(
            light,
            currentEntryIndex,
            activeEntryCount);

        entryIndex += entrySpan;

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

        // Point / Spotの影響範囲外ではDiffuseとSpecularが必ず0になる。
        // 既存のAmbient加算だけを保持し、Shadow評価とBRDF計算を省略する。
        if (attenuation <= 0.0f)
        {
            result.ambient += light.Ambient.rgb;
            continue;
        }

        float shadow = 1.0;

        if (ShouldEvaluateShadow(light))
        {
            if (light.LightType == LIGHT_TYPE_DIRECTIONAL_CSM && light.Dummy == 1)
            {
                shadow = ShadowFactorCascadesHighPrecisionFallback(
                    input.worldPos,
                    currentEntryIndex,
                    entrySpan,
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
                    input.worldPos,
                    currentEntryIndex,
                    entrySpan,
                    receiverFaceAlignment,
                    shadowParam);
            }
            else
            {
                shadow = ShadowFactor(
                    input.worldPos,
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
