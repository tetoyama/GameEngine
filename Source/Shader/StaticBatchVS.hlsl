#include "common.hlsl"

struct StaticBatchVSInput
{
    float4 Position : POSITION0;
    float4 Normal : NORMAL0;
    float4 Tangent : TANGENT0;
    float4 Diffuse : COLOR0;
    float2 TexCoord : TEXCOORD0;
    float4 InstanceWorld0 : INSTANCEWORLD0;
    float4 InstanceWorld1 : INSTANCEWORLD1;
    float4 InstanceWorld2 : INSTANCEWORLD2;
    float4 InstanceWorld3 : INSTANCEWORLD3;
    uint4 InstanceObject : INSTANCEOBJECT0;
};

struct StaticBatchVSOutput
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

void main(in StaticBatchVSInput input, out StaticBatchVSOutput output)
{
    float4x4 world = float4x4(
        input.InstanceWorld0,
        input.InstanceWorld1,
        input.InstanceWorld2,
        input.InstanceWorld3
    );

    float4 worldPosition = mul(input.Position, world);
    output.WorldPosition = worldPosition;
    output.Position = mul(mul(worldPosition, View), Projection);

    float4 worldNormal = normalize(mul(float4(input.Normal.xyz, 0.0f), world));
    float4 worldTangent = normalize(mul(float4(input.Tangent.xyz, 0.0f), world));
    output.Normal = worldNormal;
    output.Tangent = worldTangent;
    output.Bitangent = float4(
        cross(worldNormal.xyz, worldTangent.xyz) * input.Tangent.w,
        0.0f
    );

    output.Diffuse = input.Diffuse;
    float2 divisor = UVEnd - UVStart;
    divisor.x = abs(divisor.x) > 0.000001f ? divisor.x : 1.0f;
    divisor.y = abs(divisor.y) > 0.000001f ? divisor.y : 1.0f;
    output.TexCoord = UVStart + input.TexCoord / divisor;
    output.InstanceObject = input.InstanceObject;
}
