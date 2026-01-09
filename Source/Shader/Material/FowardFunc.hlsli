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
    input.baseColor = Material.Diffuse;
    if (Material.TextureEnable)
    {
        input.baseColor *=
            BaseColorTex.Sample(LinearSampler, input.uv);
    }

    input.normal = normalize(In.Normal.xyz);
    input.shininess = Material.Shininess;
    input.emission = Material.Emission.rgb;

    // ===== IDs =====
    input.materialID = ShaderID;
    input.objectID = ObjectID;
    input.sceneID = SceneID;

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
