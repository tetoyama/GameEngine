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

// ================= Environment Map =================
Texture2D EnvironmentMap : register(t7);
SamplerState EnvSampler : register(s3);

// ================= Implement =================
// GBuffer からマテリアル計算用の入力を復元する
MaterialInput GetMaterialInput(PS_IN In)
{
    MaterialInput input;
    input.uv = In.TexCoord;

    input.baseColor =
        float4(GAlbedo.Sample(PointSampler, input.uv).rgb, 1);

    input.normal =
        normalize(GNormal.Sample(PointSampler, input.uv).rgb * 2.0 - 1.0);

    uint width, height;
    GPosition.GetDimensions(width, height);
    int2 pixelCoord = int2(In.TexCoord.x * width, In.TexCoord.y * height);

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



// =====================================================
// CSM PCF サンプリングヘルパー
// =====================================================
// suvBase   : アトラス内タイル起点 UV
// depth     : 比較深度
// texelSize : タイルサイズ込みのテクセルサイズ
// stepTexel : PCF ステップ倍率 (pcf.StepTexel)
// radius    : PCF カーネル半径
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
    ShadowPCFParams pcf)
{
    const float CascadeMargin = 0.02;
    const float CascadeBlend = 0.08;

    int selectedCascade = cascadeCount - 1;
    float4 sp;

    //--------------------------------------------------
    // far cascade fallback transform
    //--------------------------------------------------

    {
        int safeIdx = min(firstLightIdx + cascadeCount - 1, LIGHT_MAX_COUNT - 1);
        LIGHT cLight = Lights[safeIdx];

        sp = mul(float4(worldPos, 1), cLight.LightView);
        sp = mul(sp, cLight.LightProjection);
    }

    //--------------------------------------------------
    // cascade selection
    //--------------------------------------------------

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

        if (all(cuv >= -CascadeMargin) && all(cuv <= 1.0 + CascadeMargin))
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

    //--------------------------------------------------
    // selected cascade shadow
    //--------------------------------------------------

    LIGHT selectedLight =
        Lights[min(firstLightIdx + selectedCascade, LIGHT_MAX_COUNT - 1)];

    float bias =
        selectedLight.Param.w * (1.0 + selectedCascade * 0.5);

    float depth = saturate(sp.z - bias);

    int tileIndex = atlasOffset + selectedCascade;

    uint grid = (uint) ceil(sqrt((float) ShadowAtlasCount));
    float tile = 1.0 / grid;

    uint gx = tileIndex % grid;
    uint gy = tileIndex / grid;

    float2 tileMin = float2(gx, gy) * tile;

    float2 texelSize;
    ShadowMap.GetDimensions(texelSize.x, texelSize.y);
    texelSize = 1.0 / texelSize;
    texelSize *= tile;

    int radius = max(pcf.KernelRadius, 0);

    float safeMargin =
        (radius * pcf.StepTexel + 1) * texelSize.x;

    uv = clamp(uv, safeMargin, 1.0 - safeMargin);

    float2 suvBase = tileMin + uv * tile;

    float shadow =
        SampleCascadePCF(
            suvBase,
            depth,
            texelSize,
            pcf.StepTexel,
            radius
        );

    //--------------------------------------------------
    // fallback shadow
    //--------------------------------------------------

    float nextShadow = shadow;

    if (selectedCascade < cascadeCount - 1)
    {
        int nextCascade = selectedCascade + 1;

        LIGHT nextLight =
            Lights[min(firstLightIdx + nextCascade, LIGHT_MAX_COUNT - 1)];

        float4 nsp =
            mul(float4(worldPos, 1), nextLight.LightView);

        nsp =
            mul(nsp, nextLight.LightProjection);

        if (nsp.w > 0.0)
        {
            nsp.xyz /= nsp.w;

            float2 nuv = nsp.xy * 0.5 + 0.5;
            nuv.y = 1.0 - nuv.y;

            if (all(nuv >= -CascadeMargin) && all(nuv <= 1.0 + CascadeMargin))
            {
                float nextBias =
                    nextLight.Param.w * (1.0 + nextCascade * 0.5);

                float ndepth =
                    saturate(nsp.z - nextBias);

                int nTileIdx = atlasOffset + nextCascade;

                uint ngx = nTileIdx % grid;
                uint ngy = nTileIdx / grid;

                float2 ntileMin =
                    float2(ngx, ngy) * tile;

                nuv = clamp(nuv, safeMargin, 1.0 - safeMargin);

                float2 nsuvBase =
                    ntileMin + nuv * tile;

                nextShadow =
                    SampleCascadePCF(
                        nsuvBase,
                        ndepth,
                        texelSize,
                        pcf.StepTexel,
                        radius
                    );
            }
        }
    }

    //--------------------------------------------------
    // cascade blend
    //--------------------------------------------------

    if (selectedCascade < cascadeCount - 1)
    {
        float edge =
            min(min(uv.x, 1.0 - uv.x), min(uv.y, 1.0 - uv.y));

        float blendWeight =
            saturate(edge / CascadeBlend);

        shadow =
            lerp(nextShadow, shadow, blendWeight);
    }

    return shadow;
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


#endif
