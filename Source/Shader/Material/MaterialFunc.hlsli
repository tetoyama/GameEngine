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

float SampleCascadeTapPrevious(
    float2 sampleUV,
    float depth,
    float2 sampleMin,
    float2 sampleMax)
{
    return ShadowMap.SampleCmpLevelZero(
        ShadowSampler,
        clamp(sampleUV, sampleMin, sampleMax),
        depth);
}

// PR #46で正常だったCSM専用PCF処理。
// CSMについては共通Atlas PCFへ統合せず、当時のTexel幅とFallbackを維持する。
// 2026-07-09: タップ座標のみ現在TileのHalf-Texel Safe Rangeへclampする
// (Shadow_Atlas_PCF_Contract §2.4)。offset幅・Texel歩幅は従来のまま変更しない。
// 2026-07-09: Sample数と結果を変えず、1x1 fast pathと3x3/5x5固定展開でCSM sample costを削減する。
float SampleCascadePCFPrevious(
    float2 suvBase,
    float depth,
    float2 texelSize,
    float stepTexel,
    int radius,
    float2 sampleMin,
    float2 sampleMax)
{
    const float2 safeBase = clamp(suvBase, sampleMin, sampleMax);
    const float2 scaledTexel = texelSize * stepTexel;

    if (radius <= 0)
    {
        return SampleCascadeTapPrevious(
            safeBase,
            depth,
            sampleMin,
            sampleMax);
    }

    if (radius == 1)
    {
        float shadow = 0.0;
        [unroll]
        for (int sy = -1; sy <= 1; sy++)
        {
            [unroll]
            for (int sx = -1; sx <= 1; sx++)
            {
                shadow += SampleCascadeTapPrevious(
                    safeBase + float2(sx, sy) * scaledTexel,
                    depth,
                    sampleMin,
                    sampleMax);
            }
        }
        return shadow / 9.0;
    }

    if (radius == 2)
    {
        float shadow = 0.0;
        [unroll]
        for (int sy = -2; sy <= 2; sy++)
        {
            [unroll]
            for (int sx = -2; sx <= 2; sx++)
            {
                shadow += SampleCascadeTapPrevious(
                    safeBase + float2(sx, sy) * scaledTexel,
                    depth,
                    sampleMin,
                    sampleMax);
            }
        }
        return shadow / 25.0;
    }

    // Material側が想定外に大きいKernelRadiusを指定した場合だけ旧式loopへFallbackする。
    float shadow = 0.0;
    int count = 0;

    [loop]
    for (int sy = -radius; sy <= radius; sy++)
    {
        [loop]
        for (int sx = -radius; sx <= radius; sx++)
        {
            shadow += SampleCascadeTapPrevious(
                safeBase + float2(sx, sy) * scaledTexel,
                depth,
                sampleMin,
                sampleMax);
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
    ShadowPCFParams pcf,
    out int debugCascadeIndex)
{
    debugCascadeIndex = -1;
    uint texW, texH;
    ShadowMap.GetDimensions(texW, texH);

    uint grid = (uint) ceil(sqrt((float) ShadowAtlasCount));
    float tile = 1.0 / grid;
    float2 texelSize = float2(1.0 / texW, 1.0 / texH) * tile;

    int radius = max(pcf.KernelRadius, 0);
    float finalShadow = 1.0;

    [unroll]
    for (int c = 0; c < DIRECTIONAL_CSM_CASCADE_COUNT; c++)
    {
        if (c >= cascadeCount)
            break;

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
        // LightingDebugCsmBiasScaleはStep19A6切り分け用の一時倍率(0=既定1.0)。
        float debugBiasScale = LightingDebugCsmBiasScale > 0.0
            ? LightingDebugCsmBiasScale
            : 1.0;
        float bias = ResolveOrthographicShadowDepthBias(
            cLight.Param.w * debugBiasScale,
            receiverNdotL);

        // Cascade Texel World Size比例の下限Bias(Step19A5契約・既定ON)。
        // 2026-07-09実機確認でFar Cascade Acneの解消を確認済み(Step19A6)。
        if ((LightingDebugFlags & LIGHTING_DEBUG_FLAG_DISABLE_CSM_TEXEL_BIAS) == 0u)
        {
            float orthoWidthScale = abs(cLight.LightProjection[0][0]);
            float ndcPerWorldZ = abs(cLight.LightProjection[2][2]);
            float tilePixels = max((float) texW * tile, 1.0);
            float texelWorldSize =
                (2.0 / max(orthoWidthScale, 0.000001)) / tilePixels;
            // Texel下限専用の調整倍率(0=既定x1)。Param.w倍率とは独立。
            float texelBiasScale = LightingDebugCsmTexelBiasScale > 0.0
                ? LightingDebugCsmTexelBiasScale
                : 1.0;
            bias = max(
                bias,
                ResolveCascadeTexelProportionalBias(
                    texelWorldSize,
                    receiverNdotL,
                    ndcPerWorldZ) * texelBiasScale);
        }

        float depth = saturate(cdepth - bias);

        int tileIndex = atlasOffset + c;
        uint gx = tileIndex % grid;
        uint gy = tileIndex / grid;
        float2 tileMin = float2(gx, gy) * tile;
        // 隣接Cascade Tileへの侵入をHalf-Texel Safe Rangeで防ぐ。
        float2 atlasTexelSize = float2(1.0 / texW, 1.0 / texH);
        float2 sampleMin = tileMin + atlasTexelSize * 0.5;
        float2 sampleMax = tileMin + tile - atlasTexelSize * 0.5;
        float2 suvBase = clamp(
            tileMin + cuv * tile,
            sampleMin,
            sampleMax);

        float shadow = SampleCascadePCFPrevious(
            suvBase,
            depth,
            texelSize,
            pcf.StepTexel,
            radius,
            sampleMin,
            sampleMax);

        if (shadow >= 1.0 && c < cascadeCount - 1)
            continue;

        if (shadow < 1.0 && c > 0)
        {
            int2 safeLoadCoord = clamp(
                int2(suvBase * float2(texW, texH)),
                int2(0, 0),
                int2((int) texW - 1, (int) texH - 1));
            int3 loadCoord = int3(safeLoadCoord, 0);
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

        debugCascadeIndex = c;
        finalShadow = min(finalShadow, shadow);
        if (0.0f >= finalShadow)
            break;
    }

    return finalShadow;
}

// Step19A6切り分け用: Cascade番号の識別色。
float3 ResolveCsmCascadeDebugColor(int cascadeIndex)
{
    if (cascadeIndex == 0) return float3(1.0, 0.2, 0.2); // 赤
    if (cascadeIndex == 1) return float3(0.2, 1.0, 0.2); // 緑
    if (cascadeIndex == 2) return float3(0.2, 0.4, 1.0); // 青
    if (cascadeIndex == 3) return float3(1.0, 1.0, 0.2); // 黄
    return float3(1.0, 0.2, 1.0); // 5段目以降: マゼンタ
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
    int csmDebugCascade = -1;

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
            if (light.LightType == LIGHT_TYPE_DIRECTIONAL_CSM && light.Dummy == 1)
            {
                int usedCascade = -1;
                // CSMはCascade選択・UV・Fallbackの安定性を優先し、
                // WorldSpace Receiver Biasで受光点を動かさない。
                // Acne対策はParam.w/CSM Texel比例Biasの比較深度側で行う。
                shadow = ShadowFactorCascadesPrevious(
                    input.worldPos,
                    currentEntryIndex,
                    entrySpan,
                    currentShadowAtlasOffset,
                    NdotL,
                    shadowParam,
                    usedCascade);
                if (usedCascade >= 0)
                    csmDebugCascade = usedCascade;
            }
            else
            {
                const float3 shadowWorldPos = ApplyShadowReceiverBias(
                    input.worldPos,
                    N,
                    L,
                    NdotL,
                    light);

                if (light.LightType == LIGHT_TYPE_POINT && light.Dummy == -1)
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

    // Step19A6切り分け用: 実際にSamplingへ採用されたCascadeを色分け表示する。
    // Shadow模様(Acne含む)が見えるよう色は50%混合に留める。
    if ((LightingDebugFlags & LIGHTING_DEBUG_FLAG_SHOW_CSM_CASCADES) != 0u &&
        csmDebugCascade >= 0)
    {
        result.diffuse = lerp(
            result.diffuse,
            ResolveCsmCascadeDebugColor(csmDebugCascade),
            0.5);
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
