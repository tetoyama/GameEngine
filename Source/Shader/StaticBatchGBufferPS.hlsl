#include "common.hlsl"
#include "commonDefine.h"

struct StaticBatchPSInput
{
    float4 Position : SV_POSITION;
    float4 Normal : NORMAL0;
    float4 Tangent : TANGENT0;
    float4 Bitangent : BINORMAL0;
    float4 Diffuse : COLOR0;
    float2 TexCoord : TEXCOORD0;
    float4 WorldPosition : TEXCOORD1;
    nointerpolation uint4 InstanceObject : TEXCOORD2;
};

struct StaticBatchGBufferOutput
{
    float4 Albedo : SV_Target0;
    float4 Normal : SV_Target1;
    float4 Position : SV_Target2;
    float4 MaterialData : SV_Target3;
    float4 Emissive : SV_Target4;
    uint4 Parameter : SV_Target5;
};

Texture2D DiffuseTexture : register(t0);
SamplerState LinearSampler : register(s0);

StaticBatchGBufferOutput main(StaticBatchPSInput input)
{
    StaticBatchGBufferOutput output;

    float4 baseColor = Material.BaseColor;
    if ((Material.MaterialFlags & MATERIAL_FLAG_USE_DIFFUSE_TEXTURE) != 0)
    {
        baseColor *= DiffuseTexture.Sample(LinearSampler, input.TexCoord);
    }
    if (baseColor.a < ALPHA_CLIP_THRESHOLD)
    {
        discard;
    }

    float3 normal = normalize(input.Normal.xyz);
    output.Albedo = baseColor;
    output.Normal = float4(normal * 0.5f + 0.5f, 1.0f);
    output.Position = float4(input.WorldPosition.xyz, 1.0f);
    output.MaterialData = float4(
        Material.Metallic,
        Material.Roughness,
        Material.AO,
        0.0f
    );
    output.Emissive = float4(
        Material.EmissiveColor,
        Material.EmissiveIntensity
    );

    output.Parameter = uint4(
        input.InstanceObject.z,
        input.InstanceObject.x,
        ShaderID,
        Material.MaterialFlags
    );
    return output;
}
