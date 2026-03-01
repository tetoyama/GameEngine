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

// ================= Implement =================
MaterialInput GetMaterialInput(PS_IN In)
{
    MaterialInput input;
    input.uv = In.TexCoord;

    input.baseColor =
        float4(GAlbedo.Sample(PointSampler, input.uv).rgb, 1);

    input.normal =
        normalize(GNormal.Sample(PointSampler, input.uv).rgb * 2.0 - 1.0);

    uint w, h;
    GPosition.GetDimensions(w, h);
    int2 pixelCoord = int2(In.TexCoord.x * w, In.TexCoord.y * h);

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

    GAlbedo.GetDimensions(input.screenSize.x, input.screenSize.y);
    input.screenUV = In.TexCoord;

    return input;
}



// =====================================================
// CSM (Cascaded Shadow Maps) シャドウファクター
// =====================================================
float ShadowFactorCSM(
    float3 worldPos,
    LIGHT  light,
    ShadowPCFParams pcf)
{
    // ---- ビュー空間深度でカスケード選択 ----
    float4 viewPos = mul(float4(worldPos, 1.0), View);
    float viewDepth = viewPos.z;

    // 遠→近の順に走査し、条件を満たす最も近いカスケードを確実に選択する。
    // [unroll]+break はコンパイラの述語化により遠いカスケードが上書きされる場合があるため、
    // break を使わず最後の書き込みが最短カスケードになるようにしている。
    int cascade = DIRECTIONAL_CSM_CASCADE_COUNT - 1; // デフォルトは最遠カスケード
    [unroll]
    for (int c = DIRECTIONAL_CSM_CASCADE_COUNT - 1; c >= 0; c--)
    {
        float splitDepth;
        if (c < 4)
            splitDepth = CsmSplitDepths[0][c];
        else
            splitDepth = CsmSplitDepths[1][c - 4];

        if (viewDepth < splitDepth)
            cascade = c;
    }

    // ---- ライトスペース変換 ----
    float4 sp = mul(float4(worldPos, 1.0), CsmViews[cascade]);
    sp = mul(sp, CsmProjections[cascade]);

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

    // ---- アトラスタイル計算 ----
    int tileIndex = CsmAtlasOffset + cascade;
    uint grid = (uint) ceil(sqrt((float) ShadowAtlasCount));
    float tile = 1.0 / grid;

    uint gx = tileIndex % grid;
    uint gy = tileIndex / grid;

    float2 tileMin = float2(gx, gy) * tile;
    float2 suvBase = tileMin + uv * tile;

    // ---- テクセルサイズ ----
    float2 texelSize;
    ShadowMap.GetDimensions(texelSize.x, texelSize.y);
    texelSize = 1.0 / texelSize;
    texelSize *= tile;

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
            float2 offset = float2(x, y) * texelSize * pcf.StepTexel;
            shadow += ShadowMap.SampleCmpLevelZero(
                ShadowSampler,
                suvBase + offset,
                depth);
            count++;
        }
    }

    return shadow / max(count, 1);
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
