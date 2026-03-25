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

    sp.xyz /= sp.w;

    float2 uv = sp.xy * 0.5 + 0.5;
    uv.y = 1.0 - uv.y;

    if (any(uv < 0.0) || any(uv > 1.0))
        return 1.0;

    // ---- シャドウバイアス (シャドウアクネ対策) ----
    // Param.w で調整可能: 値を大きくするとアクネが減り、小さくするとピーターパン現象が減る
    float bias = light.Param.w;
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

    sp.xyz /= sp.w;

    float2 uv = sp.xy * 0.5 + 0.5;
    uv.y = 1.0 - uv.y;

    if (any(uv < 0.0) || any(uv > 1.0))
        return 1.0;

    float depth = saturate(sp.z - faceLight.Param.w);
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


// =====================================================
// CSM シャドウファクター (Cascade Blending 版)
// =====================================================

float ShadowFactorCascades(
    float3 worldPos,
    int firstLightIdx,
    int cascadeCount,
    int atlasOffset,
    ShadowPCFParams pcf)
{
    const float BlendWidth = 0.08;

    //--------------------------------------------------
    // Cascade選択
    //--------------------------------------------------

    int selectedCascade = cascadeCount - 1;
    float4 sp;

    {
        int safeIdx = min(firstLightIdx + cascadeCount - 1, LIGHT_MAX_COUNT - 1);
        LIGHT cLight = Lights[safeIdx];

        sp = mul(float4(worldPos, 1), cLight.LightView);
        sp = mul(sp, cLight.LightProjection);
    }

    [loop]
    for (int c = 0; c < cascadeCount - 1; c++)
    {
        int safeIdx = min(firstLightIdx + c, LIGHT_MAX_COUNT - 1);
        LIGHT cLight = Lights[safeIdx];

        float4 csp = mul(float4(worldPos, 1), cLight.LightView);
        csp = mul(csp, cLight.LightProjection);

        if (csp.w <= 0.0)
            continue;

        float2 cuv = csp.xy / csp.w * 0.5 + 0.5;
        cuv.y = 1.0 - cuv.y;

        if (all(cuv >= 0.0) && all(cuv <= 1.0))
        {
            selectedCascade = c;
            sp = csp;
            break;
        }
    }

    if (sp.w <= 0.0)
        return 1.0;

    //--------------------------------------------------
    // UV
    //--------------------------------------------------

    sp.xyz /= sp.w;

    float2 uv = sp.xy * 0.5 + 0.5;
    uv.y = 1.0 - uv.y;

    if (any(uv < 0.0) || any(uv > 1.0))
        return 1.0;

    //--------------------------------------------------
    // selected cascade
    //--------------------------------------------------

    LIGHT selectedLight = Lights[min(firstLightIdx + selectedCascade, LIGHT_MAX_COUNT - 1)];

    float bias = selectedLight.Param.w;
    float depth = saturate(sp.z - bias);

    //--------------------------------------------------
    // atlas
    //--------------------------------------------------

    int tileIndex = atlasOffset + selectedCascade;

    uint grid = (uint) ceil(sqrt((float) ShadowAtlasCount));
    float tile = 1.0 / grid;

    uint gx = tileIndex % grid;
    uint gy = tileIndex / grid;

    float2 tileMin = float2(gx, gy) * tile;
    float2 suvBase = tileMin + uv * tile;

    //--------------------------------------------------
    // texel size
    //--------------------------------------------------

    uint texW, texH;
    ShadowMap.GetDimensions(texW, texH);

    float2 texelSize = float2(1.0 / texW, 1.0 / texH);
    texelSize *= tile;

    //--------------------------------------------------
    // PCF
    //--------------------------------------------------

    int radius = max(pcf.KernelRadius, 0);

    float shadow = SampleCascadePCF(
        suvBase,
        depth,
        texelSize,
        pcf.StepTexel,
        radius
    );

    //--------------------------------------------------
    // Cascade Blending
    //--------------------------------------------------

    if (selectedCascade < cascadeCount - 1)
    {
        float edgeDist = min(
            min(uv.x, 1.0 - uv.x),
            min(uv.y, 1.0 - uv.y)
        );

        float blend = saturate(1.0 - edgeDist / BlendWidth);

        if (blend > 0.0)
        {
            int nextCascade = selectedCascade + 1;

            LIGHT nextLight = Lights[min(firstLightIdx + nextCascade, LIGHT_MAX_COUNT - 1)];

            float4 nsp = mul(float4(worldPos, 1), nextLight.LightView);
            nsp = mul(nsp, nextLight.LightProjection);

            if (nsp.w > 0.0)
            {
                nsp.xyz /= nsp.w;

                float2 nuv = nsp.xy * 0.5 + 0.5;
                nuv.y = 1.0 - nuv.y;

                if (all(nuv >= 0.0) && all(nuv <= 1.0))
                {
                    float nextBias = nextLight.Param.w;
                    float ndepth = saturate(nsp.z - nextBias);

                    int nTileIdx = atlasOffset + nextCascade;

                    uint ngx = nTileIdx % grid;
                    uint ngy = nTileIdx / grid;

                    float2 nsuvBase = float2(ngx, ngy) * tile + nuv * tile;

                    float shadowNext = SampleCascadePCF(
                        nsuvBase,
                        ndepth,
                        texelSize,
                        pcf.StepTexel,
                        radius
                    );

                    shadow = lerp(shadow, shadowNext, blend);
                }
            }
        }
    }

    //--------------------------------------------------
    // カスケードフォールバック
    //--------------------------------------------------

    [branch]
    if (shadow >= 1.0 && selectedCascade < cascadeCount - 1)
    {
        // 修正点: 現在の座標が「選択されたカスケード(selectedCascade)」の
        // 有効なUV範囲内(0~1)にあるかチェックします。
        // すでに計算済みの uv 変数を使用します。
        bool isInCurrentCascade = all(uv >= 0.0) && all(uv <= 1.0);

        // 現在のカスケード範囲外である場合のみ、次のカスケードを確認する
        if (!isInCurrentCascade)
        {
            int nextCascade = selectedCascade + 1;

            LIGHT nextLight = Lights[min(firstLightIdx + nextCascade, LIGHT_MAX_COUNT - 1)];

            float4 nsp = mul(float4(worldPos, 1.0), nextLight.LightView);
            nsp = mul(nsp, nextLight.LightProjection);

            if (nsp.w > 0.0)
            {
                nsp.xyz /= nsp.w;

                float2 nuv = nsp.xy * 0.5 + 0.5;
                nuv.y = 1.0 - nuv.y;

                if (all(nuv >= 0.0) && all(nuv <= 1.0))
                {
                    float ndepth = saturate(nsp.z - nextLight.Param.w);

                    int nTileIdx = atlasOffset + nextCascade;

                    uint ngx = nTileIdx % grid;
                    uint ngy = nTileIdx / grid;

                    float2 nsuvBase = float2(ngx, ngy) * tile + nuv * tile;

                    shadow = min(
                        shadow,
                        SampleCascadePCF(
                            nsuvBase,
                            ndepth,
                            texelSize,
                            pcf.StepTexel,
                            radius
                        )
                    );
                }
            }
        }
    }

    return shadow;
}

#endif
