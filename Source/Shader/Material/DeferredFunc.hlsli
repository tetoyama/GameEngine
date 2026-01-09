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
Texture2D<uint4> GParam : register(t4);
SamplerState LinearSampler : register(s0);

// ================= Shadow =================
Texture2D ShadowMap : register(t5);
SamplerComparisonState ShadowSampler : register(s1);

// ================= Implement =================
MaterialInput GetMaterialInput(PS_IN In)
{
    MaterialInput input;
    input.uv = In.TexCoord;

    input.baseColor = float4(GAlbedo.Sample(LinearSampler, input.uv).rgb, 1);
    input.normal = normalize(GNormal.Sample(LinearSampler, input.uv).rgb * 2.0 - 1.0);
    input.worldPos = GPosition.Sample(LinearSampler, input.uv).rgb;
    input.viewDir = normalize(CameraPosition.xyz - input.worldPos);

    float4 mat = GMaterial.Sample(LinearSampler, input.uv);
    input.shininess = mat.r * 128.0;
    input.emission = mat.g;

    int2 pixelCoord = int2(In.Position.xy);
    uint4 param = GParam.Load(int3(pixelCoord, 0));
    input.sceneID = param.x;
    input.objectID = param.y;
    input.materialID = param.z;

    return input;
}


float ShadowFactor(
    float3 worldPos,
    LIGHT light,
    int lightIndex)
{
    // Directional / Point 共通（LightView/Projection 前提）
    float4 sp = mul(float4(worldPos, 1.0), light.LightView);
    sp = mul(sp, light.LightProjection);

    if (sp.w <= 0.0)
        return 1.0;

    sp.xyz /= sp.w;

    float2 uv = sp.xy * 0.5 + 0.5;
    uv.y = 1.0 - uv.y;

    if (any(uv < 0.0) || any(uv > 1.0))
        return 1.0;

    float depth = saturate(sp.z - 0.001);

    uint grid = (uint) ceil(sqrt((float) LIGHT_MAX_COUNT));
    float tile = 1.0 / grid;

    uint gx = lightIndex % grid;
    uint gy = lightIndex / grid;

    float2 tileMin = float2(gx, gy) * tile;
    float2 tileMax = tileMin + tile;

    float2 suv = tileMin + uv * tile;

    if (any(suv < tileMin) || any(suv > tileMax))
        return 1.0;

    return ShadowMap.SampleCmpLevelZero(ShadowSampler, suv, depth);
}

#endif
