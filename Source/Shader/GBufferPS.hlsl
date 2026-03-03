#include "common.hlsl"
#include "commonDefine.h"

struct GBUFFER_OUT
{
    float4 OutAlbedo : SV_Target0; // RGB: Albedo, A: Alpha
    float4 OutNormal : SV_Target1; // XYZ: World Normal, W: unused
    float4 OutPosition : SV_Target2; // XYZ: World Position, W: LinearDepth or 1
    float4 OutMaterial : SV_Target3; // x: Roughness, y: Metallic, z: AO, w: Flags
    float4 OutEmissive : SV_Target4; // RGB: Emissive (HDR), A: Intensity

    uint4 OutParam : SV_Target5; // ObjectID / SceneID / ShaderID / Flags
};

Texture2D DiffuseTexture : register(t0);
Texture2D NormalMap : register(t1);
SamplerState LinearSampler : register(s0);

GBUFFER_OUT main(PS_IN In)
{
    GBUFFER_OUT Out;

    // -----------------------------
    // Albedo
    // -----------------------------
    float4 baseColor = Material.BaseColor;

    // Diffuse Texture を使うか
    if ((Material.MaterialFlags & MATERIAL_FLAG_USE_DIFFUSE_TEXTURE) != 0)
    {
        baseColor *= DiffuseTexture.Sample(LinearSampler, In.TexCoord);
    }

    if (baseColor.a < 0.01f)
    {
        discard;
    }

    Out.OutAlbedo = baseColor;

    // -----------------------------
    // Normal (World Space)
    // -----------------------------
    float3 N = normalize(In.Normal.xyz);
    // Normal Texture を使うか
    if ((Material.MaterialFlags & MATERIAL_FLAG_USE_NORMAL_TEXTURE) != 0)
    {
        //N = normalize(N + NormalMap.Sample(LinearSampler, In.TexCoord).xyz * 2 - 1);
    }
    // World Normal [-1,1] → [0,1] encode
    Out.OutNormal = float4(N * 0.5f + 0.5f, 1.0f);

    // -----------------------------
    // Position (World Space)
    // -----------------------------
    Out.OutPosition.xyz = In.WorldPosition.xyz;
    Out.OutPosition.w = 1.0f;

    // -----------------------------
    // Material Parameters
    // -----------------------------
    Out.OutMaterial = float4(
        Material.Metallic,
        Material.Roughness,
        Material.AO,
        0
    );

    // -----------------------------
    // Emissive
    // -----------------------------
    // Emissive は HDR で保持する
    // Material.EmissiveColor は RGB、Material.EmissiveIntensity はスカラー
    Out.OutEmissive = float4(
        Material.EmissiveColor,
        Material.EmissiveIntensity
    );
    
    // -----------------------------
    // Param (UINT4)
    // -----------------------------
    // x : ObjectID
    // y : SceneID
    // z : ShaderID
    // w : Flags (自由)
    Out.OutParam = uint4(
        ObjectID,   // ObjectID
        SceneID,    // SceneID
        ShaderID,   // ShaderID
        Material.MaterialFlags // MaterialFlags
    );

    return Out;
}
