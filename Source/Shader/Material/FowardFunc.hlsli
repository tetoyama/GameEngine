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
// CSM シャドウファクター (アトラス統合版、Forward 用)
// LIGHT 配列から各カスケードのビュー/射影行列を直接参照する。
// CbCSM 専用バッファは不要。
// =====================================================
// firstLightIdx : LIGHT 配列内の最初のカスケードエントリインデックス
// cascadeCount  : カスケード数
// atlasOffset   : シャドウアトラス内の開始タイルインデックス
float ShadowFactorCascades(
    float3          worldPos,
    int             firstLightIdx,
    int             cascadeCount,
    int             atlasOffset,
    ShadowPCFParams pcf)
{
    // ---- 最精細カスケードを近→遠の順で UV カバレッジ判定 ----
    int  selectedCascade = cascadeCount - 1;
    float4 sp;
    {
        int safeIdx = min(firstLightIdx + cascadeCount - 1, LIGHT_MAX_COUNT - 1);
        LIGHT cLight = Lights[safeIdx];
        sp = mul(float4(worldPos, 1.0), cLight.LightView);
        sp = mul(sp, cLight.LightProjection);
    }

    [loop]
    for (int c = 0; c < cascadeCount - 1; c++)
    {
        int safeIdx = min(firstLightIdx + c, LIGHT_MAX_COUNT - 1);
        LIGHT cLight = Lights[safeIdx];
        float4 csp = mul(float4(worldPos, 1.0), cLight.LightView);
        csp = mul(csp, cLight.LightProjection);
        if (csp.w <= 0.0) continue;
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

    sp.xyz /= sp.w;
    float2 uv = sp.xy * 0.5 + 0.5;
    uv.y = 1.0 - uv.y;

    if (any(uv < 0.0) || any(uv > 1.0))
        return 1.0;

    LIGHT selectedLight = Lights[min(firstLightIdx + selectedCascade, LIGHT_MAX_COUNT - 1)];
    float bias  = selectedLight.Param.w;
    float depth = saturate(sp.z - bias);

    int   tileIndex = atlasOffset + selectedCascade;
    uint  grid      = (uint)ceil(sqrt((float)ShadowAtlasCount));
    float tile      = 1.0 / grid;
    uint  gx        = tileIndex % grid;
    uint  gy        = tileIndex / grid;
    float2 tileMin  = float2(gx, gy) * tile;
    float2 suvBase  = tileMin + uv * tile;

    float2 texelSize;
    ShadowMap.GetDimensions(texelSize.x, texelSize.y);
    texelSize = 1.0 / texelSize;
    texelSize *= tile;

    int   radius = max(pcf.KernelRadius, 0);
    float shadow = SampleCascadePCF(suvBase, depth, texelSize, pcf.StepTexel, radius);

    // ---- カスケードフォールバック ----
    [branch]
    if (shadow >= 1.0 && selectedCascade < cascadeCount - 1)
    {
        int   nextCascade = selectedCascade + 1;
        LIGHT nextLight   = Lights[min(firstLightIdx + nextCascade, LIGHT_MAX_COUNT - 1)];
        float4 nsp = mul(float4(worldPos, 1.0), nextLight.LightView);
        nsp = mul(nsp, nextLight.LightProjection);
        if (nsp.w > 0.0)
        {
            nsp.xyz /= nsp.w;
            float2 nuv = nsp.xy * 0.5 + 0.5;
            nuv.y = 1.0 - nuv.y;
            if (all(nuv >= 0.0) && all(nuv <= 1.0))
            {
                float  ndepth   = saturate(nsp.z - bias);
                int    nTileIdx = atlasOffset + nextCascade;
                uint   ngx      = nTileIdx % grid;
                uint   ngy      = nTileIdx / grid;
                float2 nsuvBase = float2(ngx, ngy) * tile + nuv * tile;

                float occRawDepth = ShadowMap.SampleLevel(LinearSampler, nsuvBase, 0).r;
                float p1_33 = nextLight.LightProjection._33;
                [branch]
                if (abs(p1_33) > 1e-6)
                {
                    float occViewZ     = (occRawDepth - nextLight.LightProjection._43) / p1_33;
                    float occNdcInFine = occViewZ * selectedLight.LightProjection._33 + selectedLight.LightProjection._43;
                    if (occNdcInFine < 0.0 || occNdcInFine > 1.0)
                    {
                        shadow = min(shadow, SampleCascadePCF(nsuvBase, ndepth, texelSize, pcf.StepTexel, radius));
                    }
                }
            }
        }
    }

    return shadow;
}


#endif
