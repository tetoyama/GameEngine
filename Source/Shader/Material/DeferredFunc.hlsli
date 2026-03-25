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


#endif
