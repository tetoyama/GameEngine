#include "common.hlsl"

struct GBUFFER_OUT
{
    float4 Albedo : SV_Target0; // RGB: Albedo, A: Alpha
    float4 Normal : SV_Target1; // XYZ: World Normal, W: unused
    float4 Position : SV_Target2; // XYZ: World Position, W: LinearDepth or 1
    float4 Material : SV_Target3; // x: Roughness, y: Metallic, z: AO, w: Emissive
    uint4 Param : SV_Target4; // ObjectID / SceneID / ShaderID / Flags
};

Texture2D DiffuseTexture : register(t0);
SamplerState LinearSampler : register(s0);

GBUFFER_OUT main(PS_IN In)
{
    GBUFFER_OUT Out;

    // -----------------------------
    // Albedo
    // -----------------------------
    float4 baseColor = Material.Diffuse;

    if (Material.TextureEnable)
    {
        float4 texColor = DiffuseTexture.Sample(LinearSampler, In.TexCoord);
        baseColor *= texColor;
    }

    Out.Albedo = baseColor;

    // -----------------------------
    // Normal (World Space)
    // -----------------------------
    float3 N = normalize(In.Normal.xyz);
    // World Normal [-1,1] → [0,1] encode
    Out.Normal = float4(N * 0.5f + 0.5f, 1.0f);

    // -----------------------------
    // Position (World Space)
    // -----------------------------
    Out.Position.xyz = In.WorldPosition.xyz;
    Out.Position.w = 1.0f;

    // -----------------------------
    // Material Parameters
    // -----------------------------
    Out.Material = float4(
        Material.Metallic,
        Material.Roughness,
        Material.AO,
        Material.Emissive
    );

    // -----------------------------
    // Param (UINT4)
    // -----------------------------
    // x : ObjectID
    // y : SceneID
    // z : ShaderID
    // w : Flags (自由)
    Out.Param = uint4(
        ObjectID,   // ObjectID
        SceneID,    // SceneID
        ShaderID,   // ShaderID
        0 // Flags
    );

    return Out;
}