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
    // Normal (World Space) - computed first for env map
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
    // Albedo
    // -----------------------------
    float4 baseColor = Material.BaseColor;

    // Environment Map (SkyBox テクスチャを反射方向でサンプリング)
    if ((Material.MaterialFlags & MATERIAL_FLAG_USE_ENV_MAP) != 0)
    {
        float3 V = normalize(CameraPosition.xyz - In.WorldPosition.xyz);
        float3 R = reflect(-V, N);

        // Equirectangular マッピング: 反射方向 → UV
        // U: +Z 方向が中心 (Blender→DirectX 座標変換に対応)
        // V: 反射の仰角を圧縮し、水平線付近の雲や空の特徴が見えるようにする
        float2 envUV;
        envUV.x = atan2(R.x, R.z) / (2.0f * PI) + 0.5f;
        float scaledY = R.y * 0.45f; // 仰角を圧縮して水平線〜雲の範囲を反射に含める
        envUV.y = acos(clamp(scaledY, -1.0f, 1.0f)) / PI;

        float4 envColor = DiffuseTexture.Sample(LinearSampler, envUV);

        // 環境色をそのままアルベドに格納（Fresnel は WaterSurfaceShader で適用）
        baseColor = envColor;
    }
    // Diffuse Texture を使うか
    else if ((Material.MaterialFlags & MATERIAL_FLAG_USE_DIFFUSE_TEXTURE) != 0)
    {
        baseColor *= DiffuseTexture.Sample(LinearSampler, In.TexCoord);
    }

    Out.OutAlbedo = baseColor;

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
        0 // Flags
    );

    return Out;
}